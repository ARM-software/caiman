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

#if defined(SUPPORT_DAQ)

#include "NiDaq.h"

#include "Logging.h"

// Support can be compiled out - in this case there is no
// source dependency on the NI DAQmx Base header, and this
// class doesn't exist at all.

NiDaq::NiDaq(const char *outputPath, FILE *binfile, Fifo *fifo) : Device(outputPath, binfile, fifo) {
  mIsRunning = false;
  mDllsLoaded = false;
  mDaqMx = DAQmxFuncs::getInstance();
}

NiDaq::~NiDaq() {
  stop();
}

void NiDaq::init(const char *device) {
  // Auto lookup device name if necessary
  mDev[MAX_DEVICE_LEN-1] = 0;
  if (!device) {
    lookup_daq();
  } else {
    strncpy(mDev, device, MAX_DEVICE_LEN);
  }

  logg->logMessage("Creating DAQ task .. this takes a while ..");

  // First call to any mDaqMx method is slow when using DAQmx Base as it initializes everything
  // Create our task
  if (!mDaqMx->createTask("caiman_task")) {
    mDaqMx->handleError("CreateTask");
  }

  // Out vendor name includes this devices serial number
  unsigned long sn;
  // This is our first DAQ call .. return a non-cryptic error if it fails
  if (!mDaqMx->getDevSerialNum(mDev, &sn)) {
    mDaqMx->handleFriendlyError("Could not get the serial number - is the DAQ connected?");
  }
  char vendor[80];
  snprintf(vendor, 80, "National Instruments S/N 0x%08x", (unsigned int)sn);

  // Configure enabled channels
  enableChannels();
  logg->logMessage("Number of fields is %d, with %d DAQ channels enabled", mNumFields, mDaqChannels);

  if (!mDaqMx->cfgSampClkTiming("", mSampleRate, mWindow)) {
    mDaqMx->handleError("CfgSampClkTiming");
  }

  logg->logMessage("DAQ has been initialized");
}

void NiDaq::start() {
  if (!mDaqMx->startTask()) {
    mDaqMx->handleError("StartTask");
  }

  mIsRunning = true;
}

void NiDaq::stop() {
  if (mIsRunning) {
    mIsRunning = false;
    if (!mDaqMx->stopTask()) {
      mDaqMx->handleError("StopTask");
    }
  }

  if (mDaqMx != NULL) {
    if (!mDaqMx->clearTask()) {
      mDaqMx->handleError("ClearTask");
    }
    mDaqMx = NULL;
  }
}

void NiDaq::processBuffer() {
  static const int BUF_SIZE = mWindow * MAX_CHANNELS;
  double data[BUF_SIZE];
  long read = 0;

  // Read DAQ data
  // DAQmx_Val_GroupByScanNumber (interleaved), or DAQmx_Val_GroupByChannel
  if (!mDaqMx->readAnalogF64(mWindow, 1.0, data, BUF_SIZE, &read, NULL)) {
    mDaqMx->handleError("ReadAnalogF64");
  }

  // Parse it, write it.
  for (int row=0; row < read; row++) {
    int value;
    unsigned int col = 0, outidx = 0;
    unsigned char outbuf[MAX_CHANNELS * EMETER_DATA_SIZE * MAX_FIELDS];

    for (int chan = 0; chan < MAX_CHANNELS; chan++) {
      double v, i;
      if (!mFields[chan]) {
	continue;
      }

      v = data[(row*mDaqChannels) + (col*2) + 0];
      i = data[(row*mDaqChannels) + (col*2) + 1];
      i = (i * 1000.0) / ((double)gSessionData.mResistors[chan]);  // i=v/r (and scale mOhms -> Ohms)
      col++;

      // Emeter always outputs enabled fields in the order POWER, VOLTAGE, CURRENT
      if (mFields[chan] & POWER) {
	value = (int)(v*i*1000.0);    // mW
	value = (value < 0)?0:value;
	outbuf[outidx++] = value & 0xFF;
	outbuf[outidx++] = (value >> 8) & 0xFF;
	outbuf[outidx++] = (value >> 16) & 0xFF;
	outbuf[outidx++] = (value >> 24) & 0xFF;
      }
      if (mFields[chan] & VOLTAGE) {
	value = (int)(v*1000.0);     // mV
	value = (value < 0)?0:value;
	outbuf[outidx++] = value & 0xFF;
	outbuf[outidx++] = (value >> 8) & 0xFF;
	outbuf[outidx++] = (value >> 16) & 0xFF;
	outbuf[outidx++] = (value >> 24) & 0xFF;
      }
      if (mFields[chan] & CURRENT) {
	value = (int)(i*1000.0);     // mA
	value = (value < 0)?0:value;
	outbuf[outidx++] = value & 0xFF;
	outbuf[outidx++] = (value >> 8) & 0xFF;
	outbuf[outidx++] = (value >> 16) & 0xFF;
	outbuf[outidx++] = (value >> 24) & 0xFF;
      }
    } // for each channel
    writeData(outbuf, outidx);
  } // for each row
}

void NiDaq::lookup_daq() {
  mDev[0] = '\0';
  if (!mDaqMx->getSysDevNames(mDev, MAX_DEVICE_LEN)) {
    logg->logError("Auto discovery of DAQ device name is not supported. "
		   "Please specify a device with -d.");
    handleException();
  }

  if (mDev[0] == '\0') {
    logg->logError("Device could not be found, please verify the device is attached or specify a device with -d.");
    handleException();
  }
}

void NiDaq::prepareChannels() {
  int index;

  for (index = 0; index < MAX_CHANNELS; index ++) {
    mFields[index] = 0;
  }

  // Similar to the EnergyMeter:
  // for every counter in the configuration counter, collate the fields we want to turn on.
  mNumFields = 0;
  for (index = 0; index < MAX_COUNTERS; index++) {
    if (!gSessionData.mCounterEnabled[index]) {
      continue;
    }

    if (!(mFields[gSessionData.mCounterChannel[index]] & gSessionData.mCounterField[index])) {
      // increment mNumFields if field not already accounted for
      mNumFields++;
    }
    // bitwise OR all types on a per channel basis
    mFields[gSessionData.mCounterChannel[index]] |= gSessionData.mCounterField[index];
  }

  // Write captured.xml before initializing the device for live as initializing
  // the device may take some time. (We preserve the emeter's 4 byte data size)
  mVendor = "National Instruments";
  mDatasize = EMETER_DATA_SIZE;
}

void NiDaq::enableChannels() {
  int index;

  // Work out if we've been told which DAQ channel to use
  char *daq_channel[MAX_CHANNELS][MAX_FIELDS_PER_CHANNEL];
  memset(&daq_channel, 0, sizeof(daq_channel));
  for (index = 0; index < MAX_COUNTERS; index++) {
    if (!gSessionData.mCounterEnabled[index]) {
      continue;
    }

    int chan = gSessionData.mCounterChannel[index];
    int field = gSessionData.mCounterField[index];

    if (field & VOLTAGE) {
      field = mVoltageField;
    } else if (field & CURRENT) {
      field = mCurrentField;
    } else {
      continue;          // DAQ channel not supported for POWER fields
    }

    daq_channel[chan][field] = get_channel_info(gSessionData.mCounterDaqCh[index], field, chan);
  } // for index over MAX_COUNTERS

  // The next part is different for the DAQ. For every channel where ANY field is on,
  // collect BOTH V & I. (No 'power' channel for DAQ.)
  // The output routine will combine for power, and output only enabled fields.

  // We hard code the voltage range for current channels to +/- 200mV, and for voltage
  // channels to +/- 5V. (This fits NI USB-621x ranges, but of course not all DAQs.)
  // All channels configured in differential mode, DAQmx_Val_Diff.
  // Later on, we might add fields for V range and RSE (DAQmx_Val_RSE) to configuration.xml

  mDaqChannels = 0;
  for (index = 0; index < MAX_CHANNELS; index++) {
    if (!mFields[index]) {
      continue;
    }

    char *chan;

    chan = daq_channel[index][mVoltageField];
    if (chan != NULL) {
      logg->logMessage("Configuring DAQ '%s' as differential 0-5V (Voltage) channel", chan);
      if (!mDaqMx->createAIVoltageChan(chan, NULL, 0.0, 5.0, NULL)) {
	mDaqMx->handleError(chan);
      }
    }

    chan = daq_channel[index][mCurrentField];
    if (chan != NULL) {
      logg->logMessage("Configuring DAQ '%s' as differential +-200mV (Current) channel", chan);
      if (!mDaqMx->createAIVoltageChan(chan, NULL, -0.2, 0.2, NULL)) {
	mDaqMx->handleError(chan);
      }
    }

    mDaqChannels += 2;
  } // for all channels
}

// Figures out daq channel name
//  - Uses daq_ch from configuration.xml if configured, combined with device name
//  - Otherwise, defaults to chan * 2 + [0|1], depending on V or I
//  - Updates configured channel string with the device name
//  - Returns that updated configuration string
// This routine can be updated to populate a struct from configured DAQ channel information,
// including DAQ V range to be used, and RSE | Differential
char *NiDaq::get_channel_info(char *config_chan, int field, int chan) {
  char ch_str[MAX_STRING_LEN];
  ch_str[MAX_STRING_LEN-1] = 0;

  if (config_chan[0]) {
    if (strstr(config_chan,"/")) {
      return config_chan;
    }
    snprintf(ch_str, MAX_STRING_LEN, "%s/%s", mDev, config_chan);
  } else {
    snprintf(ch_str, MAX_STRING_LEN, "%s/ai%d", mDev, (chan * 2) + field); // ai0, 2, etc for V, 1, 3, etc for I
  }

  strcpy(config_chan, ch_str);
  return config_chan;
}

#endif
