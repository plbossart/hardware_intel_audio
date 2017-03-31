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

#include "AudioEffect.hpp"
#include <hardware/audio_effect.h>
#include <AudioNonCopyable.hpp>
#include <list>
#include <utils/Errors.h>

class AudioEffect;

class AudioEffectSession : private audio_comms::utilities::NonCopyable
{
private:
    typedef std::list<AudioEffect *>::iterator EffectListIterator;
    typedef std::list<AudioEffect *>::const_iterator EffectListConstIterator;

public:
    AudioEffectSession(uint32_t sessionId);

    /**
     * Add an effect. It does not attach the effect to the session. It helps the session
     * knowing what effects are available, and being able to serve request to create an effect
     * referred by its uuid and handle.
     *
     * @param[in] effect Audio Effect to add.
     *
     * @return OK is success, error code otherwise.
     */
    android::status_t addEffect(AudioEffect *effect);

    /**
     * Create an effect. It attaches the effect to the session
     *
     * @param[in] uuid unique identifier of the audio effect.
     * @param[in] interface handle of the audio effect.
     *
     * @return OK is success, error code otherwise.
     */
    android::status_t createEffect(const effect_uuid_t *uuid, effect_handle_t *interface);

    /**
     * Set the IO handle attached to the session.
     *
     * @param[in] effect Audio Effect to add.
     *
     * @return OK is success, error code otherwise.
     */
    android::status_t removeEffect(AudioEffect *effect);

    /**
     * Set the IO handle attached to the session.
     *
     * @param[in] ioHandle IO handle to attach to the session.
     */
    void setIoHandle(int ioHandle);

    /**
     * Get the IO handle attached to the session.
     *
     * @return IO handle.
     */
    int getIoHandle() const { return mIoHandle; }

    /**
     * Get the effect session Id.
     *
     * @return session id.
     */
    uint32_t getId() const { return mId; }

    static const int mSessionNone = -1; /**< No session tag. */

private:
    /**
     * Function to be used as the predicate in find_if call.
     */
    struct MatchUuid : public std::binary_function<AudioEffect *, const effect_uuid_t *, bool>
    {

        bool operator()(const AudioEffect *effect,
                        const effect_uuid_t *uuid) const
        {
            return memcmp(effect->getUuid(), uuid, sizeof(effect_uuid_t)) == 0;
        }
    };
    /**
     * Initialise the effect session.
     * It resets the io handle attached to it and the audio source.
     */
    void init();

    /**
     * Retrieve effect by its UUID
     *
     * @param[in] uuid unique identifier of the audio effect.
     *
     * @return valid effect pointer if found, NULL otherwise.
     */
    AudioEffect *findEffectByUuid(const effect_uuid_t *uuid);

    int mIoHandle; /**< handle of input stream this session is linked to. */
    uint32_t mId; /**< Id of the sessions.*/
    audio_source_t  mSource; /**< Audio Source (ie Use Case) associated to this session. */

    std::list<AudioEffect *> mEffectsCreatedList; /**< created pre processors. */
    std::list<AudioEffect *> mEffectsActiveList; /**< active pre processors - for later use?. */
    std::list<AudioEffect *> mEffectsList; /**< Effects in the sessions. */

};
