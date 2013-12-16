/**
 * Copyright (C) ARM Limited 2011-2013. All rights reserved.
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

#ifndef ENERGYPROBE_H
#define ENERGYPROBE_H

#include "Devices.h"

class EnergyProbe : public Device {
public:
  EnergyProbe(const char *outputPath, FILE *binfile, Fifo *fifo);
  virtual ~EnergyProbe();

  virtual void prepareChannels();
  virtual void init(const char *devicename);
  virtual void start();
  virtual void stop();
  virtual void processBuffer();

private:
  int readAll(char *ptr, size_t size);     // returns number of bytes read
  void readAck();
  void readString(char *buffer, int limit);
  int writeAll(char *ptr, size_t size);    // returns num bytes written
  void writeChar(char c);
  void syncToDevice();
  void enableChannels();

  // Returns pointer to device string
  char* autoDetectDevice();

  // OS-specific device autodetect function
  void autoDetectDevice_OS(char *comport, int buffersize);

  // Initialized on construction
  bool mIsRunning;

  // Initialized on init()
  DEVICE mStream;
  const char *mComport;
  char mFields[MAX_EPROBE_CHANNELS];

  // Intentionally unimplemented
  EnergyProbe(const EnergyProbe &);
  EnergyProbe &operator=(const EnergyProbe &);
};

#endif // ENERGYPROBE_H
