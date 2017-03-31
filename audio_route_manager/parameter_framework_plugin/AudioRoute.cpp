/*
 * INTEL CONFIDENTIAL
 *
 * Copyright (c) 2013-2016 Intel Corporation All Rights Reserved.
 *
 * The source code contained or described herein and all documents related to
 * the source code ("Material") are owned by Intel Corporation or its suppliers
 * or licensors.
 *
 * Title to the Material remains with Intel Corporation or its suppliers and
 * licensors. The Material contains trade secrets and proprietary and
 * confidential information of Intel or its suppliers and licensors. The
 * Material is protected by worldwide copyright and trade secret laws and treaty
 * provisions. No part of the Material may be used, copied, reproduced,
 * modified, published, uploaded, posted, transmitted, distributed, or disclosed
 * in any way without Intel's prior express written permission.
 *
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be
 * express and approved by Intel in writing.
 */

#include "AudioRoute.hpp"
#include "Tokenizer.h"
#include "RouteMappingKeys.hpp"
#include "RouteSubsystem.hpp"
#include <AudioCommsAssert.hpp>

using intel_audio::IRouteInterface;
using std::string;

const string AudioRoute::mOutputDirection = "out";
const string AudioRoute::mStreamType = "streamRoute";
const string AudioRoute::mPortDelimiter = "-";

const AudioRoute::Status AudioRoute::mDefaultStatus = {
    .isApplicable = false,
    .needReconfigure = false,
    .needReroute = false
};

AudioRoute::AudioRoute(const std::string &mappingValue,
                       CInstanceConfigurableElement *instanceConfigurableElement,
                       const CMappingContext &context,
                       core::log::Logger &logger)
    : CFormattedSubsystemObject(instanceConfigurableElement,
                                logger,
                                mappingValue,
                                MappingKeyAmend1,
                                (MappingKeyAmendEnd - MappingKeyAmend1 + 1),
                                context),
      mRouteSubsystem(static_cast<const RouteSubsystem *>(
                          instanceConfigurableElement->getBelongingSubsystem())),
      mRouteInterface(mRouteSubsystem->getRouteInterface()),
      mStatus(mDefaultStatus),
      mIsStreamRoute(context.getItem(MappingKeyType) == mStreamType),
      mIsOut(context.getItem(MappingKeyDirection) == mOutputDirection)
{
    mRouteName = getFormattedMappingValue();

    string ports = context.getItem(MappingKeyPorts);
    Tokenizer mappingTok(ports, mPortDelimiter);
    std::vector<string> subStrings = mappingTok.split();
    AUDIOCOMMS_ASSERT(subStrings.size() <= mDualPorts,
                      "Route cannot be connected to more than 2 ports");

    string portSrc = subStrings.size() >= mSinglePort ? subStrings[0] : string();
    string portDst = subStrings.size() == mDualPorts ? subStrings[1] : string();

    // Append route to RouteMgr with route root name
    if (mIsStreamRoute) {
        mRouteInterface->addAudioStreamRoute(context.getItem(MappingKeyAmend1),
                                             portSrc, portDst, mIsOut);
    } else {
        mRouteInterface->addAudioRoute(context.getItem(MappingKeyAmend1), portSrc, portDst, mIsOut);
    }
}

bool AudioRoute::sendToHW(string & /*error*/)
{
    Status status;

    // Retrieve blackboard
    blackboardRead(&status, sizeof(status));

    // Updates applicable status if changed
    if (status.isApplicable != mStatus.isApplicable) {

        mRouteInterface->setRouteApplicable(mRouteName, status.isApplicable);
    }

    // Updates reconfigure flag if changed
    if (status.needReconfigure != mStatus.needReconfigure) {

        mRouteInterface->setRouteNeedReconfigure(mRouteName, status.needReconfigure);
    }

    // Updates reroute flag if changed
    if (status.needReroute != mStatus.needReroute) {

        mRouteInterface->setRouteNeedReroute(mRouteName, status.needReroute);
    }

    mStatus = status;

    return true;
}
