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
#pragma once

#include <AudioNonCopyable.hpp>
#include <stdint.h>
#include <string>

namespace intel_audio
{

class RoutingElement : public audio_comms::utilities::NonCopyable
{
public:
    RoutingElement(const std::string &name);
    virtual ~RoutingElement() {}

    /**
     * Returns identifier of current routing element
     *
     * @returns string representing the name of the routing element
     */
    const std::string &getName() const { return mName; }

    virtual void resetAvailability() {}

private:
    /** Unique Identifier of a routing element */
    std::string mName;
};

} // namespace intel_audio
