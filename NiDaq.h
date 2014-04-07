/**
 * Copyright (C) ARM Limited 2011-2014. All rights reserved.
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

#ifndef NIDAQ_H
#define NIDAQ_H

#include "Devices.h"

#include "DAQmxFuncs.h"

class NiDaq : public Device {
public:
  NiDaq(const char *outputPath, FILE *binfile, Fifo *fifo);
  virtual ~NiDaq();

  virtual void prepareChannels();
  virtual void init(const char *device);
  virtual void start();
  virtual void stop();
  virtual void processBuffer();

private:
  void enableChannels();
  void lookup_daq();
  char *get_channel_info(char *config_chan, int field, int chan);

  static const int mWindow = mSampleRate/10;

  static const int mVoltageField = 0;  // Used to determine DAQ channel numbering.
  static const int mCurrentField = 1;  // (Default is Voltage channel first.)

  // Initialized on construction
  bool mIsRunning;
  bool mDllsLoaded;

  // Initialized on init
  DAQmxFuncs *mDaqMx;
  static const int MAX_DEVICE_LEN = 120;
  char mDev[MAX_DEVICE_LEN];
  int mDaqChannels;

  char mFields[MAX_CHANNELS];

  // Intentionally unimplemented
  NiDaq(const NiDaq &);
  NiDaq &operator=(const NiDaq &);
};

#endif // NIDAQ_H
