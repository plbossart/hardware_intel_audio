/*
 * Copyright (C) 2013-2016 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "AudioConversion"

#include "AudioConversion.hpp"
#include "AudioConverter.hpp"
#include "AudioReformatter.hpp"
#include "AudioRemapper.hpp"
#include "AudioResampler.hpp"
#include "AudioUtils.hpp"
#include <AudioCommsAssert.hpp>
#include <utilities/Log.hpp>
#include <media/AudioBufferProvider.h>
#include <stdlib.h>

using audio_comms::utilities::Log;
using namespace android;
using namespace std;

namespace intel_audio
{

const uint32_t AudioConversion::mMaxRate = 92000;

const uint32_t AudioConversion::mMinRate = 8000;

const uint32_t AudioConversion::mAllocBufferMultFactor = 2;

AudioConversion::AudioConversion()
    : mConvOutBufferIndex(0),
      mConvOutFrames(0),
      mConvOutBufferSizeInFrames(0),
      mConvOutBuffer(NULL)
{
    mAudioConverter[ChannelCountSampleSpecItem] = new AudioRemapper(ChannelCountSampleSpecItem);
    mAudioConverter[FormatSampleSpecItem] = new AudioReformatter(FormatSampleSpecItem);
    mAudioConverter[RateSampleSpecItem] = new AudioResampler(RateSampleSpecItem);
}

AudioConversion::~AudioConversion()
{
    for (int i = 0; i < NbSampleSpecItems; i++) {

        delete mAudioConverter[i];
        mAudioConverter[i] = NULL;
    }

    free(mConvOutBuffer);
    mConvOutBuffer = NULL;
}

bool AudioConversion::supportConversion(const SampleSpec &ssSrc, const SampleSpec &ssDst)
{
    return supportReformat(ssSrc.getFormat(), ssDst.getFormat()) &&
           supportRemap(ssSrc.getChannelCount(), ssDst.getChannelCount()) &&
           supportResample(ssSrc.getSampleRate(), ssDst.getSampleRate());
}
bool AudioConversion::supportReformat(audio_format_t srcFormat, audio_format_t dstFormat)
{
    return AudioReformatter::supportReformat(srcFormat, dstFormat);
}

bool AudioConversion::supportRemap(uint32_t srcChannels, uint32_t dstChannels)
{
    return AudioRemapper::supportRemap(srcChannels, dstChannels);
}

bool AudioConversion::supportResample(uint32_t srcRate, uint32_t dstRate)
{
    return AudioResampler::supportResample(srcRate, dstRate);
}

status_t AudioConversion::configure(const SampleSpec &ssSrc, const SampleSpec &ssDst)
{
    status_t ret = NO_ERROR;

    emptyConversionChain();

    free(mConvOutBuffer);

    mConvOutBuffer = NULL;
    mConvOutBufferIndex = 0;
    mConvOutFrames = 0;
    mConvOutBufferSizeInFrames = 0;

    mSsSrc = ssSrc;
    mSsDst = ssDst;

    if (ssSrc == ssDst) {
        Log::Debug() << __FUNCTION__ << ": no convertion required";
        return ret;
    }

    Log::Debug() << __FUNCTION__ << ": SOURCE rate=" << ssSrc.getSampleRate()
                 << " format=" << static_cast<int32_t>(ssSrc.getFormat())
                 << " channels=" << ssSrc.getChannelCount();
    Log::Debug() << __FUNCTION__ << ": DST rate=" << ssDst.getSampleRate()
                 << " format=" << static_cast<int32_t>(ssDst.getFormat())
                 << " channels=" << ssDst.getChannelCount();

    SampleSpec tmpSsSrc = ssSrc;

    // Start by adding the remapper, it will add consequently the reformatter and resampler
    // This function may alter the source sample spec
    ret = configureAndAddConverter(ChannelCountSampleSpecItem, &tmpSsSrc, &ssDst);
    if (ret != NO_ERROR) {

        return ret;
    }
    return tmpSsSrc == ssDst ? OK : INVALID_OPERATION;
}

status_t AudioConversion::getConvertedBuffer(void *dst,
                                             const size_t outFrames,
                                             AudioBufferProvider *bufferProvider)
{
    if (!bufferProvider || !dst) {
        Log::Error() << __FUNCTION__ << ": Invalid buffer";
        return BAD_VALUE;
    }

    status_t status = NO_ERROR;

    if (mActiveAudioConvList.empty()) {
        Log::Error() << __FUNCTION__ << ": conversion called with empty converter list";
        return NO_INIT;
    }

    //
    // Realloc the Output of the conversion if required (with margin of the worst case)
    //
    if (mConvOutBufferSizeInFrames < outFrames) {

        mConvOutBufferSizeInFrames = outFrames +
                                     (mMaxRate / mMinRate) * mAllocBufferMultFactor;
        int16_t *reallocBuffer =
            static_cast<int16_t *>(realloc(mConvOutBuffer,
                                           mSsDst.convertFramesToBytes(
                                               mConvOutBufferSizeInFrames)));
        if (reallocBuffer == NULL) {
            Log::Error() << __FUNCTION__ << ": (frames=" << outFrames << " ): realloc failed";
            return NO_MEMORY;
        }
        mConvOutBuffer = reallocBuffer;
    }

    size_t framesRequested = outFrames;

    //
    // Frames are already available from the ConvOutBuffer, empty it first!
    //
    if (mConvOutFrames) {

        size_t frameToCopy = min(framesRequested, mConvOutFrames);
        framesRequested -= frameToCopy;
        mConvOutBufferIndex += mSsDst.convertFramesToBytes(frameToCopy);
    }

    //
    // Frames still needed? (_pConvOutBuffer emptied!)
    //
    while (framesRequested != 0) {

        //
        // Outputs in the convOutBuffer
        //
        AudioBufferProvider::Buffer &buffer(mConvInBuffer);

        // Calculate the frames we need to get from buffer provider
        // (Runs at ssSrc sample spec)
        // Note that is is rounded up.
        buffer.frameCount = AudioUtils::convertSrcToDstInFrames(framesRequested, mSsDst, mSsSrc);

        //
        // Acquire next buffer from buffer provider
        //
        status = bufferProvider->getNextBuffer(&buffer);
        if (status != NO_ERROR) {

            return status;
        }

        //
        // Convert
        //
        size_t convertedFrames;
        char *convBuf = reinterpret_cast<char *>(mConvOutBuffer) + mConvOutBufferIndex;
        status = convert(buffer.raw, reinterpret_cast<void **>(&convBuf),
                         buffer.frameCount, &convertedFrames);
        if (status != NO_ERROR) {

            bufferProvider->releaseBuffer(&buffer);
            return status;
        }

        mConvOutFrames += convertedFrames;
        mConvOutBufferIndex += mSsDst.convertFramesToBytes(convertedFrames);

        size_t framesToCopy = min(framesRequested, convertedFrames);
        framesRequested -= framesToCopy;

        //
        // Release the buffer
        //
        bufferProvider->releaseBuffer(&buffer);
    }

    //
    // Copy requested outFrames from the output buffer of the conversion.
    // Move the remaining frames to the beginning of the convOut buffer
    //
    memcpy(dst, mConvOutBuffer, mSsDst.convertFramesToBytes(outFrames));
    mConvOutFrames -= outFrames;

    //
    // Move the remaining frames to the beginning of the convOut buffer
    //
    if (mConvOutFrames) {

        memmove(mConvOutBuffer,
                reinterpret_cast<char *>(mConvOutBuffer) + mSsDst.convertFramesToBytes(outFrames),
                mSsDst.convertFramesToBytes(mConvOutFrames));
    }

    // Reset buffer Index
    mConvOutBufferIndex = 0;

    return NO_ERROR;
}

status_t AudioConversion::convert(const void *src,
                                  void **dst,
                                  const size_t inFrames,
                                  size_t *outFrames)
{
    if (!src) {
        Log::Error() << __FUNCTION__ << ": NULL source buffer";
        return BAD_VALUE;
    }
    const void *srcBuf = src;
    void *dstBuf = NULL;
    size_t srcFrames = inFrames;
    size_t dstFrames = 0;
    status_t status = NO_ERROR;

    if (mActiveAudioConvList.empty()) {

        // Empty converter list -> No need for convertion
        // Copy the input on the ouput if provided by the client
        // or points on the imput buffer
        if (*dst) {

            memcpy(*dst, src, mSsSrc.convertFramesToBytes(inFrames));
            *outFrames = inFrames;
        } else {

            *dst = (void *)src;
            *outFrames = inFrames;
        }
        return NO_ERROR;
    }

    AudioConverterListIterator it;
    for (it = mActiveAudioConvList.begin(); it != mActiveAudioConvList.end(); ++it) {

        AudioConverter *pConv = *it;
        dstBuf = NULL;
        dstFrames = 0;

        if (*dst && (pConv == mActiveAudioConvList.back())) {

            // Last converter must output within the provided buffer (if provided!!!)
            dstBuf = *dst;
        }
        status = pConv->convert(srcBuf, &dstBuf, srcFrames, &dstFrames);
        if (status != NO_ERROR) {

            return status;
        }

        srcBuf = dstBuf;
        srcFrames = dstFrames;
    }

    *dst = dstBuf;
    *outFrames = dstFrames;

    return status;
}

void AudioConversion::emptyConversionChain()
{
    mActiveAudioConvList.clear();
}

status_t AudioConversion::doConfigureAndAddConverter(SampleSpecItem sampleSpecItem,
                                                     SampleSpec *ssSrc,
                                                     const SampleSpec *ssDst)
{
    SampleSpec tmpSsDst = *ssSrc;
    tmpSsDst.setSampleSpecItem(sampleSpecItem, ssDst->getSampleSpecItem(sampleSpecItem));

    if (sampleSpecItem == ChannelCountSampleSpecItem) {

        tmpSsDst.setChannelsPolicy(ssDst->getChannelsPolicy());
    }

    status_t ret = mAudioConverter[sampleSpecItem]->configure(*ssSrc, tmpSsDst);
    if (ret != NO_ERROR) {

        return ret;
    }
    mActiveAudioConvList.push_back(mAudioConverter[sampleSpecItem]);
    *ssSrc = tmpSsDst;

    return NO_ERROR;
}

status_t AudioConversion::configureAndAddConverter(SampleSpecItem sampleSpecItem,
                                                   SampleSpec *ssSrc,
                                                   const SampleSpec *ssDst)
{
    if (sampleSpecItem >= NbSampleSpecItems) {
        Log::Error() << __FUNCTION__ << ": Sample Spec item out of range";
        return INVALID_OPERATION;
    }
    // If the input format size is higher, first perform the reformat
    // then add the resampler
    // and perform the reformat (if not already done)
    if (ssSrc->getSampleSpecItem(sampleSpecItem) > ssDst->getSampleSpecItem(sampleSpecItem)) {

        status_t ret = doConfigureAndAddConverter(sampleSpecItem, ssSrc, ssDst);
        if (ret != NO_ERROR) {

            return ret;
        }
    }

    if ((sampleSpecItem + 1) < NbSampleSpecItems) {
        // Dive
        status_t ret = configureAndAddConverter((SampleSpecItem)(sampleSpecItem + 1), ssSrc,
                                                ssDst);
        if (ret != NO_ERROR) {

            return ret;
        }
    }

    // Handle the case of destination sample spec item is higher than input sample spec
    // or destination and source channels policy are different
    if (!SampleSpec::isSampleSpecItemEqual(sampleSpecItem, *ssSrc, *ssDst)) {

        return doConfigureAndAddConverter(sampleSpecItem, ssSrc, ssDst);
    }
    return NO_ERROR;
}
}  // namespace intel_audio
