/**
 * Copyright (C) ARM Limited 2011-2015. All rights reserved.
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

#include "Devices.h"

#include <stdlib.h>

#include "Fifo.h"
#include "Logging.h"

Device::Device(const char *outputPath, FILE* binfile, Fifo *fifo) : mOutputPath(outputPath), mBinfile(binfile), mFifo(fifo) {
  if (fifo != NULL) {
    mBuffer = fifo->start();
  }
}

Device::~Device() {
}

char *Device::getXML(int *const length) const {
  const int BUF_SIZE = 1<<14;
  char *const xml = (char *)malloc(BUF_SIZE);
  int pos = 0;

  pos += snprintf(&xml[pos], BUF_SIZE - pos, "<?xml version=\"1.0\" encoding='UTF-8'?>\n");
  pos += snprintf(&xml[pos], BUF_SIZE - pos, "<captured version=\"%d\">\n", CAIMAN_VERSION);
  pos += snprintf(&xml[pos], BUF_SIZE - pos, "  <target name=\"%s\" sample_rate=\"%d\" sources=\"%d\" size=\"%d\"/>\n", mVendor, mSampleRate, mNumFields, mDatasize);
  pos += snprintf(&xml[pos], BUF_SIZE - pos, "  <counters>\n");
  for (int i = 0; i < MAX_COUNTERS; i++) {
    if (gSessionData.mCounterEnabled[i]) {
      pos += snprintf(&xml[pos], BUF_SIZE - pos, "    <counter source=\"%d\" channel=\"%d\" type=\"%s\" resistance=\"%d\"/>\n", gSessionData.mCounterSource[i], gSessionData.mCounterChannel[i], field_title_names[gSessionData.mCounterField[i]], gSessionData.mResistors[gSessionData.mCounterChannel[i]]);
    }
  }
  pos += snprintf(&xml[pos], BUF_SIZE - pos, "  </counters>\n");
  pos += snprintf(&xml[pos], BUF_SIZE - pos, "</captured>\n");
  xml[pos] = '\0';

  *length = pos;
  return xml;
}

void Device::writeXML() const {
  FILE *xmlout;
  char filename[CAIMAN_PATH_MAX + 1];

  snprintf(filename, CAIMAN_PATH_MAX, "%scaptured.xml", mOutputPath);
  if ((xmlout = fopen(filename, "wt")) == NULL) {
    logg->logError("Unable to create %s", filename);
    handleException();
  }

  int length;
  char * xml = getXML(&length);
  fputs(xml, xmlout);
  free(xml);
  fclose(xmlout);
}

void Device::writeData(void *buf, size_t size) {
  if (mBinfile != NULL) {
    if (fwrite(buf, 1, size, mBinfile) != size) {
      logg->logError("Error writing .apc energy data");
      handleException();
    }
  } else {
    memcpy(mBuffer, buf, size);
    mBuffer = mFifo->write(size);
  }
}
