/**
 * Copyright (C) Arm Limited 2011-2016. All rights reserved.
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

#ifndef SESSION_DATA_H
#define SESSION_DATA_H

#include "Config.h"

// This is an informational version only; no compatibility is performed based on this value
#define CAIMAN_VERSION 651
// Differentiates development versions (timestamp) from release versions
#define PROTOCOL_DEV 10000000

#define MAX_EPROBE_CHANNELS     3
#define MAX_DAQ_CHANNELS        40  // A caiman 'channel' includes V+I

#define MAX_CHANNELS MAX_DAQ_CHANNELS
#define MAX_FIELDS_PER_CHANNEL 3
#define MAX_FIELDS MAX_CHANNELS * MAX_FIELDS_PER_CHANNEL
#define MAX_COUNTERS MAX_FIELDS * 2 // one for peak, one for average
#define MAX_STRING_LEN 80
#define MAX_DESCRIPTION_LEN 400

// Fields
static const char * const field_title_names[] = { "", "Power", "Voltage", "", "Current" };
enum
{
    POWER = 1,
    VOLTAGE = 2,
    CURRENT = 4
};

class SessionData
{
public:
    SessionData();
    ~SessionData();
    void initialize();
    void compileData();

    // Counters
    // one of power, voltage, or current
    int mCounterField[MAX_COUNTERS];
    // channel 0, 1, or 2
    int mCounterChannel[MAX_COUNTERS];
    // which source of data emitted from the energy probe, 0-8
    int mCounterSource[MAX_COUNTERS];
    // DAQ Channel, such as 'ai1', 'ai2', etc.
    char mCounterDaqCh[MAX_COUNTERS][MAX_STRING_LEN];
    // whether this counter is enabled
    bool mCounterEnabled[MAX_COUNTERS];

    // scale factor based on a 0.1 ohm shunt resistor
    float mSourceScaleFactor[MAX_FIELDS];

    // whether this channel is enabled
    bool mChannelEnabled[MAX_CHANNELS];
    // shunt resistor
    int mResistors[MAX_CHANNELS];

    int mMaxEnabledChannel;
};

extern SessionData gSessionData;

#endif // SESSION_DATA_H
