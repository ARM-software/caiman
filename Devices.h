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

#ifndef devices_h
#define devices_h

#include <stdio.h>

#if defined(WIN32)
#include <windows.h>
#define DEVICE     HANDLE
#else
#define DEVICE     int
#endif

#include "SessionData.h"

class Fifo;

#define EMETER_DATA_SIZE    4

class Device {
public:
  // Constructed with an output path, which must stay allocated by the caller
  // for the life of this object ..
  Device(const char *output_path, FILE *binfile, Fifo *fifo);
  virtual ~Device();

  virtual void prepareChannels() = 0;
  virtual void init(const char *devicename) = 0;
  virtual void start() = 0;
  virtual void stop() = 0;
  virtual void processBuffer() = 0;

  char *getXML(int *const length) const;
  void writeXML() const;

protected:
  void writeData(void *buf, size_t size);

  static const unsigned int mSampleRate = 10000;
  int mNumFields;
  const char *mVendor;
  int mDatasize;

private:
  const char *mOutputPath;
  FILE *const mBinfile;
  Fifo *const mFifo;
  char *mBuffer;

  // Intentionally unimplemented
  Device(const Device &);
  Device &operator=(const Device &);
};

#endif
