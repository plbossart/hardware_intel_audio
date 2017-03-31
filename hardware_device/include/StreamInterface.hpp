/*
 * Copyright (C) 2014-2015 Intel Corporation
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

#pragma once

#include <hardware/audio.h>
#include <utils/Errors.h>
#include <string>


namespace intel_audio
{

/** Stream interface common to input and output streams. */
class StreamInterface
{
public:
    virtual ~StreamInterface() {}

    /**
     * @return the sampling rate in Hz - eg. 44100.
     */
    virtual uint32_t getSampleRate() const = 0;

    /** Change the sampling rate.
     * Currently unused - use set_parameters with key AUDIO_PARAMETER_STREAM_SAMPLING_RATE
     *
     * @param[in] rate
     * @return OK if succeed, error code else.
     */
    virtual android::status_t setSampleRate(uint32_t rate) = 0;

    /**
     * @return size of input/output buffer in bytes for this stream - eg. 4800.
     *         It should be a multiple of the frame size.
     *         @see also DeviceInterface::getInputBufferSize.
     */
    virtual size_t getBufferSize() const = 0;

    /**
     * @return the channel mask - e.g. AUDIO_CHANNEL_OUT_STEREO or AUDIO_CHANNEL_IN_STEREO
     */
    virtual audio_channel_mask_t getChannels() const = 0;

    /**
     * @return the audio format - e.g. AUDIO_FORMAT_PCM_16_BIT
     */
    virtual audio_format_t getFormat() const = 0;

    /** Change the audio format.
     * Currently unused - use set_parameters with key AUDIO_PARAMETER_STREAM_FORMAT
     *
     * @param[in] format
     * @return OK if succeed, error code else.
     */
    virtual android::status_t setFormat(audio_format_t format) = 0;

    /** Put the audio hardware input/output into standby mode.
     * Driver should exit from standby mode at the next I/O operation.
     *
     * @return OK if succeed, error code else.
     */
    virtual android::status_t standby() = 0;

    /** Dump the state of the audio input/output device.
     *
     * @param[in] fd file descriptor used as dump output.
     * @return OK if succeed, error code else.
     */
    virtual android::status_t dump(int fd) const = 0;

    /**
     * @return the set of device(s) which this stream is connected to.
     */
    virtual audio_devices_t getDevice() const = 0;

    /** Set the set of device(s) which this stream is connected to.
     *
     * @param[in] device set which this stream is connected to.
     * @return OK if succeed, error code else.
     *
     * NOTE: Currently unused - corresponds to setParameters() with key
     *       AUDIO_PARAMETER_STREAM_ROUTING for both input and output.
     *       AUDIO_PARAMETER_STREAM_INPUT_SOURCE is an additional information used by
     *       input streams only.
     */
    virtual android::status_t setDevice(audio_devices_t device) = 0;

    /** Get audio stream parameters.
     *
     * @param[in] keys list of parameter key value pairs in the form: key1;key2;key3;...
     *            Some keys are reserved for standard parameters (@see AudioParameter class)
     * @return list of parameter key value pairs in the form: key1=value1;key2=value2;...
     */
    virtual std::string getParameters(const std::string &keys) const = 0;

    /** Set audio stream parameters.
     * The audio flinger will put the stream in standby and then change the
     * parameter value.
     *
     * @param[in] keyValuePairs list of parameter key value pairs in the form:
     *            key1=value1;key2=value2;...
     *            Some keys are reserved for standard parameters (@see AudioParameter class)
     * @return OK if succeed, error code else.
     *         If the implementation does not accept a parameter change while the output is active
     *         but the parameter is acceptable otherwise, it must return -ENOSYS.
     */
    virtual android::status_t setParameters(const std::string &keyValuePairs) = 0;

    /** Add an effect to the stream audio processing chain.
     * When calling this function, the stream must be already attached to an audio route.
     *
     * @param[in] effect structure of the effect to add.
     * @return OK if succeed, error code else.
     */
    virtual android::status_t addAudioEffect(effect_handle_t effect) = 0;

    /** Remove an effect to the stream audio processing chain.
     * When calling this function, the stream must be still attached to an audio route.
     *
     * @param[in] effect structure of the effect to add.
     * @return OK if succeed, error code else.
     */
    virtual android::status_t removeAudioEffect(effect_handle_t effect) = 0;
};

/** Audio output stream interface. */
class StreamOutInterface : public virtual StreamInterface
{
public:
    /**
     * @return the audio hardware driver estimated latency in milliseconds.
     */
    virtual uint32_t getLatency() = 0;

    /** Set the offloaded volume.
     * Use this method in situations where audio mixing is done in the
     * hardware. This method serves as a direct interface with hardware,
     * allowing you to directly set the volume as apposed to via the framework.
     * This method might produce multiple PCM outputs or hardware accelerated
     * codecs, such as MP3 or AAC.
     *
     * @param[in] left volume
     * @param[in] right volume
     * @return OK if succeed, error code else.
     */
    virtual android::status_t setVolume(float left, float right) = 0;

    /** Write audio buffer to driver.
     * If set_callback() has previously been called to enable non-blocking mode
     * the write() is not allowed to block. It must write only the number of
     * bytes that currently fit in the driver/hardware buffer and then return
     * this byte count. If this is less than the requested write size the
     * callback function must be called when more space is available in the
     * driver/hardware buffer.
     *
     * @param[in] buffer containing data to write.
     * @param[in,out] bytes inputs size to write; outputs written byte count.
     * @return OK if succeed, error code else.
     */
    virtual android::status_t write(const void *buffer, size_t &bytes) = 0;

    /** Get frame count written to the DAC.
     *
     * @param[out] dspFrames number of audio frames written by the audio dsp to DAC since
     *             the output has exited standby.
     * @return OK if succeed, error code else.
     */
    virtual android::status_t getRenderPosition(uint32_t &dspFrames) const = 0;

    /** Get the local time at which the next write to the audio driver will be presented.
     *
     * @param timestamp in microseconds (where the epoch is decided by the local audio HAL)
     * @return OK if succeed, error code else.
     */
    virtual android::status_t getNextWriteTimestamp(int64_t &timestamp) const = 0;


    /** Notifies to the audio driver to flush the queued data.
     * Stream must already be paused before calling flush().
     *
     * @return OK if succeed, error code else.
     *
     * NOTE: Implementation of this function is mandatory for offloaded playback.
     */
    virtual android::status_t flush() = 0;

    /** Set the callback function for notifying completion of non-blocking write and drain.
     * Calling this function implies that all future write() and drain()
     * must be non-blocking and use the callback to signal completion.
     *
     * @param callback function
     * @param cookie context
     * @return OK if succeed, error code else.
     */
    virtual android::status_t setCallback(stream_callback_t callback, void *cookie) = 0;

    /** Notifies to the audio driver to stop playback however the queued buffers are
     * retained by the hardware.
     * Useful for implementing pause/resume.
     * Empty implementation if not supported however should be implemented for hardware
     * with non-trivial latency. In the pause state audio hardware could still be using power.
     * User may consider calling suspend after a timeout.
     *
     * @return OK if succeed, error code else.
     *
     * NOTE: Implementation of this function is mandatory for offloaded playback.
     */
    virtual android::status_t pause() = 0;

    /** Notifies to the audio driver to resume playback following a pause.
     *
     * @return OK if succeed, error code else.
     *
     * NOTE: Implementation of this function is mandatory for offloaded playback.
     */
    virtual android::status_t resume() = 0;

    /** Requests notification when data buffered by the driver/hardware has been played.
     * If set_callback() has previously been called to enable non-blocking mode,
     * the drain() must not block, instead it should return quickly and completion
     * of the drain is notified through the callback.
     * If set_callback() has not been called, the drain() must block until
     * completion.
     * If type==AUDIO_DRAIN_ALL, the drain completes when all previously written
     * data has been played.
     * If type==AUDIO_DRAIN_EARLY_NOTIFY, the drain completes shortly before all
     * data for the current track has played to allow time for the framework
     * to perform a gapless track switch.
     * Drain must return immediately on stop() and flush() call
     *
     * @param type @see audio_drain_type_t for possible values.
     *             AUDIO_DRAIN_ALL, the drain completes when all previously written
     *                 data has been played.
     *             AUDIO_DRAIN_EARLY_NOTIFY, the drain completes shortly before all
     *                 data for the current track has played to allow time for the framework
     *                 to perform a gapless track switch.
     * @return OK if succeed, error code else.
     *
     * NOTE: Implementation of this function is mandatory for offloaded playback.
     */
    virtual android::status_t drain(audio_drain_type_t type) = 0;

    /** Get recent count of the number of audio frames presented to an external observer.
     * This excludes frames which have been written but are still in the pipeline.
     * The count is not reset to zero when output enters standby.
     * The returned count is expected to be 'recent',
     * but does not need to be the most recent possible value.
     * However, the associated time should correspond to whatever count is returned.
     * Example:  assume that N+M frames have been presented, where M is a 'small' number.
     * Then it is permissible to return N instead of N+M,
     * and the timestamp should correspond to N rather than N+M.
     * The terms 'recent' and 'small' are not defined.
     * They reflect the quality of the implementation.
     *
     * @param frames recent count of the number of audio frames presented.
     * @param timestamp value of CLOCK_MONOTONIC as of this presentation count.
     * @return OK if succeed, error code else.
     *
     * NOTE: 3.0 and higher only.
     */
    virtual android::status_t getPresentationPosition(uint64_t &frames,
                                                      struct timespec &timestamp) const = 0;
};

/** Audio input stream interface. */
class StreamInInterface : public virtual StreamInterface
{
public:
    /** Set the input gain for the audio driver.
     *
     * @param gain to set
     * @return OK if succeed, error code else.
     *
     * NOTE: This method is for for future use.
     */
    virtual android::status_t setGain(float gain) = 0;

    /** Read audio buffer from audio driver.
     *
     * @param buffer to be used to store read data
     * @param[in,out] bytes inputs buffer length; outputs read byte count.
     * @return OK if succeed, error code else.
     */
    virtual android::status_t read(void *buffer, size_t &bytes) = 0;

    /** Get lost frame count.
     * Audio driver is expected to reset the value to 0 and restart counting
     * upon returning the current value by this function call.
     * Such loss typically occurs when the user space process is blocked
     * longer than the capacity of audio driver buffers.
     *
     * @return the number of input frames lost in the audio driver since the
     *         last call of this function.
     */
    virtual uint32_t getInputFramesLost() const = 0;


    /**
     * Return a recent count of the number of audio frames received and
     * the clock time associated with that frame count.
     *
     * @param frames is the total frame count received. This should be as early in
     *        the capture pipeline as possible. In general,
     *        frames should be non-negative and should not go "backwards".
     *
     * @param time is the clock MONOTONIC time when frames was measured. In general,
     *     time should be a positive quantity and should not go "backwards".
     *
     * @return OK if succeed, error code else: -ENOSYS if the device is not
     *         ready/available, or -EINVAL if the arguments are null or otherwise invalid.
     */
    virtual android::status_t getCapturePosition(int64_t &frames, int64_t &time) = 0;
};

} // namespace intel_audio
