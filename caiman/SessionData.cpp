/**
 * Copyright (C) ARM Limited 2011-2016. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "SessionData.h"

#include "Logging.h"

SessionData gSessionData;

SessionData::SessionData()
{
    initialize();
}

SessionData::~SessionData()
{
}

void SessionData::initialize()
{
    for (int i = 0; i < MAX_COUNTERS; i++) {
        mCounterField[i] = 0;
        mCounterChannel[i] = 0;
        mCounterSource[i] = 0;
        mCounterEnabled[i] = false;

    }

    for (int i = 0; i < MAX_CHANNELS; i++) {
        mChannelEnabled[i] = false;
        mResistors[i] = 0;
    }

    mMaxEnabledChannel = -1;
}

void SessionData::compileData()
{
    static bool compiled = false;

    if (compiled)
        return;

    int channelsConfigured = 0;
    for (int channel = 0; channel < MAX_CHANNELS; ++channel) {
        // Is a resistor value given for each enabled channel?
        if (mResistors[channel] <= 0) {
            continue;
        }

        mChannelEnabled[channel] = true;

        for (int field = 0; field < 3; ++field) {
            static const int field_num[] = { POWER, CURRENT, VOLTAGE };
            static const int field_source[] = { 0, 2, 1 };

            const int index = 3 * channelsConfigured + field;
            mCounterChannel[index] = channel;
            mCounterField[index] = field_num[field];
            mCounterDaqCh[index][0] = '\0';
            mCounterEnabled[index] = true;

            // Determine sources
            // Source always in the following order, as per energy meter device:
            // ch0 pwr, ch0 volt, ch0 curr, ch1 pwr, ch1 volt, ch1 curr, ch2 pwr, etc.
            mCounterSource[index] = 3 * channelsConfigured + field_source[field];
            if (mCounterField[index] == POWER || mCounterField[index] == CURRENT) {
                mSourceScaleFactor[mCounterSource[index]] = 100 / (float) mResistors[channel];
            }
            else {
                mSourceScaleFactor[mCounterSource[index]] = 1;
            }
        }

        ++channelsConfigured;

        if (channel > mMaxEnabledChannel) {
            mMaxEnabledChannel = channel;
        }
    }

    if (mMaxEnabledChannel < 0) {
        logg.logError("No channels enabled, please ensure resistance values are set");
        handleException();
    }

    compiled = true;
}
