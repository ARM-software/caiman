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

#include "EnergyProbe.h"

#include <stdlib.h>

#if defined(WIN32)
#include <setupapi.h>
#else
#include <unistd.h>
#include <fcntl.h>
#if defined(__linux__) && defined(SUPPORT_UDEV)
#include <libudev.h>
#endif
#endif

#include "Dll.h"
#include "Logging.h"

#if defined(WIN32)
#define DEVICE                  HANDLE
#define OPEN_DEVICE(x)          CreateFile(x, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL)
#define READ_DEVICE(x,y,z,n)    ReadFile(x, y, z, (LPDWORD)&n, NULL)
#define WRITE_DEVICE(x,y,z,n)   WriteFile(x, y, z, (LPDWORD)&n, NULL)
#define CLOSE_DEVICE(x)         CloseHandle(x)
#else
// Linux or DARWIN
#define INVALID_HANDLE_VALUE    -1
#define DEVICE                  int
#define OPEN_DEVICE(x)          open(x, O_RDWR | O_SYNC)
#define READ_DEVICE(x,y,z,n)    (n = read(x, y, z))
#define WRITE_DEVICE(x,y,z,n)   (n = write(x, y, z))
#define CLOSE_DEVICE(x)         close(x)
#define tHANDLE                 pthread_t
#endif

// This is a compatibility version between caiman and the ARM Energy Probe
#define ENERGY_PROBE_VERSION 20110803

#define DYNAMIC_LINK_UDEV 1

// Main.cpp defines Quit. It's ugly, but it's true
extern volatile bool gQuit;

// Defines for Energy Probe.

// Commands to the energy probe
#define CMD_VERSION     1
#define CMD_VENDOR      3
#define CMD_RATE        5
#define CMD_CONFIG      7
#define CMD_START       9
#define CMD_STOP        0x0b
#define CMD_RESET       0xff

// Responses from the energy probe
#define RESP_ACK        0xac

// Define channels
#define EN_POWER        (1<<0)
#define EN_VOLTAGE      (1<<1)
#define EN_CURRENT      (1<<2)
#define EMETER_BUFFER_SIZE  64

// Public interface implementation

EnergyProbe::EnergyProbe(const char *outputPath, FILE *binfile, Fifo *fifo) : Device(outputPath, binfile, fifo) {
  mIsRunning = false;
}

EnergyProbe::~EnergyProbe() {
  // May be called normally, on controlled sigInt, on exception
  if (mIsRunning) {
    stop();
  }
}

void EnergyProbe::init(const char *devicename) {
  // Automatic detection of the comport, unless specified on the commandline
  mComport = NULL;
  mComport = (devicename == NULL)? autoDetectDevice() : devicename;

#ifdef __linux__
  // Set device to raw mode (remove interaction with line discipline)
  char command[80];
  snprintf(command, sizeof(command) - 1, "stty -F %s raw", mComport);
  if (system(command) != 0) {
    logg->logError("Unable to set %s to raw mode, please verify the device exists", mComport);
    handleException();
  }
#endif

  if (mComport == NULL || *mComport == 0) {
    logg->logError("Unable to detect the energy probe. Verify that it is attached to the computer and properly enumerated with the OS. If it is enumerated, you can override auto-detection by specifying the 'Device' in the options dialog.");
    handleException();
  }

  // Make connection to the energy metering device
  mStream = OPEN_DEVICE(mComport);
  if (mStream == INVALID_HANDLE_VALUE) {
    logg->logError("Unable to open the energy probe at %s - consider overriding auto-detection by specifying the 'Device' in the options dialog.", mComport);
    handleException();
  }

  // Sync and reset the interface, then read data until the magic sequence is found
  syncToDevice();

  // Get and check the version
  unsigned int version;
  writeChar(CMD_VERSION);
  readAll((char*)&version, 4);
  logg->logMessage("energy probe version is %d", version);
  if (version != ENERGY_PROBE_VERSION) {
    // Version mismatch
    logg->logError("energy probe version '%d' is different than the supported version '%d'.\n>> Please upgrade the caiman app and energy probe firmware to the latest versions.", version, ENERGY_PROBE_VERSION);
    handleException();
  }

  // Get the vendor
  char vendor[80];
  writeChar(CMD_VENDOR);
  readString(vendor, sizeof(vendor));
  logg->logMessage("energy probe vendor is %s", vendor);

  // Get the sample rate
  unsigned int sampleRate;
  writeChar(CMD_RATE);
  readAll((char*)&sampleRate, 4);
  logg->logMessage("energy probe sample rate is %d", sampleRate);
  if (sampleRate != mSampleRate) {
    logg->logError("Unexpected sample rate %i, expected %i\n", sampleRate, mSampleRate);
    handleException();
  }

  // Enable the channels
  enableChannels();
  logg->logMessage("Number of fields is %d", mNumFields);
}

void EnergyProbe::start() {
  writeChar(CMD_START);

  mIsRunning = true;
}

void EnergyProbe::stop() {

  // Regardless of whether the command succeeds - no point in
  // trying to stop again on exception, if we couldn't write
  // the stop this time around
  mIsRunning = false;

  // Stop the device and do not search for an ACK as it will be mixed with the data
  char c = CMD_STOP;
  int n;
  WRITE_DEVICE(mStream, &c, 1, n);

  CLOSE_DEVICE(mStream);

  // Silence variable ‘n’ set but not used warning
  (void)n;
}

void EnergyProbe::processBuffer() {
  // Was 1024, now 64+8 .. +8 padding shouldn't be needed
  static char inBuffer[EMETER_BUFFER_SIZE + 8];
  int inLength = readAll(inBuffer, EMETER_BUFFER_SIZE);

  char data1, data2, data3, data4;
  unsigned int outLength = 0;
  int location = 0;
  unsigned short inframe;
  static int remaining = 0;
  static unsigned short outframe = 0;
  static unsigned char last_value[MAX_FIELDS][EMETER_DATA_SIZE] = {{0}};
  static char outBuffer[2*EMETER_BUFFER_SIZE];

  while (location < inLength) {
    if (remaining == 0) {
      // read frame
      data1 = inBuffer[location++];
      data2 = inBuffer[location++];
      inframe = (unsigned char)data1 + ((unsigned char)data2 << 8);
      // output missing frames
      if (outframe != inframe) {
	logg->logMessage("Missing frames %d-%d (%d frames)", outframe, inframe, inframe-outframe);
      }
      while (outframe != inframe) {
	outframe++;
	for (int index = 0; index < mNumFields; index++) {
	  outBuffer[outLength++] = last_value[index][0];
	  outBuffer[outLength++] = last_value[index][1];
	  outBuffer[outLength++] = last_value[index][2];
	  outBuffer[outLength++] = last_value[index][3];

	  // write data
	  if (outLength >= sizeof(outBuffer)) {
	    writeData(outBuffer, outLength);
	    outLength = 0;
	  }
	}
      } // while outframe != inframe
      outframe++;
      // mNumFields data fields should follow the frame
      remaining = mNumFields;
    } else {
      // read data
      data1 = inBuffer[location++];
      data2 = inBuffer[location++];

      // account for scale factor of different shunt resistors
      int value = (unsigned char)data1 + ((unsigned char)data2 << 8);
      value = (int)((float)value * gSessionData.mSourceScaleFactor[mNumFields - remaining]);
      // Check for overflow
      if (value & ~0x7FFFFFFF) {
	value = 0x7FFFFFFF;
	static bool neverOverflowed = true;
	if (neverOverflowed) {
	  neverOverflowed = false;
	  logg->logMessage("Power overflow detected");
	}
      }
      data1 = value & 0xFF;
      data2 = (value >> 8) & 0xFF;
      data3 = (value >> 16) & 0xFF;
      data4 = (value >> 24) & 0xFF;

      // output data
      outBuffer[outLength++] = data1;
      outBuffer[outLength++] = data2;
      outBuffer[outLength++] = data3;
      outBuffer[outLength++] = data4;
      // save data
      last_value[mNumFields - remaining][0] = data1;
      last_value[mNumFields - remaining][1] = data2;
      last_value[mNumFields - remaining][2] = data3;
      last_value[mNumFields - remaining][3] = data4;
      // update remaining data fields
      remaining--;

      // write data
      if (outLength >= sizeof(outBuffer)) {
	writeData(outBuffer, outLength);
	outLength = 0;
      }
    }
  }

  if (location != inLength) {
    logg->logMessage("INVESTIGATE: misaligned length");
  }

  // write data
  writeData(outBuffer, outLength);
}

int EnergyProbe::readAll(char *ptr, size_t size) {
  int remain = size, n;
  while (!gQuit && remain > 0) {
    if (!READ_DEVICE(mStream, ptr, remain, n)) {
      logg->logError("Error reading from the energy probe; data will be incomplete");
      handleException();
    }
    remain -= n;
    ptr += n;
  }
  return size - remain;
}

void EnergyProbe::readAck() {
  bool found = false;
  unsigned char ack;

  // Discard all zeros; error on a char other than RESP_ACK
  while (!found) {
    readAll((char*)&ack, sizeof(ack));

    if (ack == 0) {
      continue;
    } else if (ack == RESP_ACK) {
      found = true;
    } else {
      logg->logError("Expected an ack from device but received %02x", ack);
      handleException();
    }
  }
}

void EnergyProbe::readString(char *buffer, int limit) {
  while (--limit) {
    char ch;
    readAll(&ch, 1);
    *buffer++ = ch;
    if (!ch) {
      *buffer = 0;
      break;
    }
  }
}

int EnergyProbe::writeAll(char *ptr, size_t size) {
  int remain = size, n;

  while (!gQuit && remain > 0) {
    if (!WRITE_DEVICE(mStream, ptr, remain, n)) {
      logg->logError("Write failure when communicating with the energy probe");
      handleException();
    }
    remain -= n;
    ptr += n;
  }

  if (remain != 0) {
    return size - remain;
  }

  return size;
}

void EnergyProbe::writeChar(char c) {
  writeAll(&c, 1);
  readAck();
}

void EnergyProbe::syncToDevice() {
  unsigned char sync[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, CMD_RESET};
  int found = 0;
  unsigned char byte;

  writeAll((char*)sync, sizeof(sync));

  // Repeat until eight 0xFFs are found in a row
  logg->logMessage("Read the energy probe for the magic sequence");
  while (1) {
    readAll((char*)&byte, sizeof(byte));

    if (byte == 0xff) {
      if (++found == 8) {
	break;
      }
    } else {
      found = 0;
    }
  }
  logg->logMessage("Sync successful and magic detected on the energy probe");
}

void EnergyProbe::prepareChannels() {
  int index;

  // Energy Probe supports fewer channels than are permitted in configuration
  // Check user hasn't over-configured.
  if (gSessionData.mMaxEnabledChannel >= MAX_EPROBE_CHANNELS) {
    logg->logError("Incorrect configuration: channel %d is configured, but ARM Energy Probe supports ch0-ch%d.",
		   gSessionData.mMaxEnabledChannel, MAX_EPROBE_CHANNELS-1);
    handleException();
  }

  mNumFields = 0;
  for (index = 0; index < MAX_EPROBE_CHANNELS; index ++) {
    mFields[index] = 0;
  }

  for (index = 0; index < MAX_COUNTERS; index++) {
    if (gSessionData.mCounterEnabled[index]) {
      if (!(mFields[gSessionData.mCounterChannel[index]] & gSessionData.mCounterField[index])) {
	// increment mNumFields if field not already accounted for
	mNumFields++;
      }
      // bitwise OR all types on a per channel basis
      mFields[gSessionData.mCounterChannel[index]] |= gSessionData.mCounterField[index];
    }
  }

  // Write captured.xml
  mVendor = "ARM Streamline Energy Probe";
  mDatasize = EMETER_DATA_SIZE;
}

void EnergyProbe::enableChannels() {
  int index;

  for (index = 0; index < MAX_EPROBE_CHANNELS; index++) {
    char command[] = {CMD_CONFIG, 0, 0};
    command[1] = index;
    command[2] = mFields[index];
    writeAll(command, sizeof(command));
    readAck();
  }
}

#if defined(WIN32)

inline void EnergyProbe::autoDetectDevice_OS(char *comport, int buffersize) {
  HDEVINFO devInfo = SetupDiGetClassDevs(NULL, "USB", NULL, DIGCF_ALLCLASSES | DIGCF_PRESENT);
  if (devInfo == INVALID_HANDLE_VALUE) {
    logg->logError("Detection of COM port failed, please verify the device is attached or specify the COM port on the command line to override auto detection");
    handleException();
  }

  SP_DEVINFO_DATA spDevInfoData;
  spDevInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
  for (int index = 0; SetupDiEnumDeviceInfo(devInfo, index, &spDevInfoData); index++) {
    int size = 0;
    char buf[MAX_PATH];

    if (!SetupDiGetDeviceInstanceId(devInfo, &spDevInfoData, buf, sizeof(buf), (PDWORD)&size)) {
      logg->logMessage("SetupDiGetDeviceInstanceId() error");
      continue;
    }

    // 1fc9/0003 is the prototype device; 0d28/0004 is the release device
    if (strstr(buf, "USB\\VID_0D28&PID_0004") == NULL && strstr(buf, "USB\\VID_1FC9&PID_0003") == NULL) {
      // Device not a match
      continue;
    }

    // lookup registry properties
    int data;
    if (SetupDiGetDeviceRegistryProperty(devInfo, &spDevInfoData, SPDRP_FRIENDLYNAME, (PDWORD)&data, (PBYTE)comport, buffersize, (PDWORD)&buffersize) == 0) {
      SetupDiDestroyDeviceInfoList(devInfo);
      logg->logError("Property listing of device failed, please verify the device is attached or specify the COM port on the command line to override auto detection");
      handleException();
    }

    // Extract COM port
    char* start = strstr(comport, "(COM");
    char* end = strstr(start, ")");
    *end = 0;
    snprintf(comport, buffersize, "\\\\.\\%s", (char*)((int)start+1)); // Required to support comports > 9, see http://msdn.microsoft.com/en-us/library/aa363858%28v=vs.85%29.aspx
    logg->logMessage("Device detected on %s", comport);
    break;
  }

  // cleanup
  SetupDiDestroyDeviceInfoList(devInfo);
}

#elif defined(__linux__) && defined(SUPPORT_UDEV)

inline void EnergyProbe::autoDetectDevice_OS(char *comport, int buffersize) {
  bool device_found = false;
  struct udev *udev;
  struct udev_enumerate *enumerate;
  struct udev_list_entry *devices, *dev_list_entry;

#ifdef DYNAMIC_LINK_UDEV
  DLLHANDLE udev_handle = NULL;
  typedef struct udev * (*udev_new_func)();
  udev_new_func udev_new_ptr = NULL;
#define udev_new() (*udev_new_ptr)();
  typedef struct udev_enumerate * (*udev_enumerate_new_func)(struct udev *);
  udev_enumerate_new_func udev_enumerate_new_ptr = NULL;
#define udev_enumerate_new(udev) (*udev_enumerate_new_ptr)(udev)
  typedef int (*udev_enumerate_add_match_subsystem_func)(struct udev_enumerate *, const char *);
  udev_enumerate_add_match_subsystem_func udev_enumerate_add_match_subsystem_ptr = NULL;
#define udev_enumerate_add_match_subsystem(udev_enumerate, subsystem) (*udev_enumerate_add_match_subsystem_ptr)(udev_enumerate, subsystem)
  typedef int (*udev_enumerate_scan_devices_func)(struct udev_enumerate *);
  udev_enumerate_scan_devices_func udev_enumerate_scan_devices_ptr = NULL;
#define udev_enumerate_scan_devices(udev_enumerate) (*udev_enumerate_scan_devices_ptr)(udev_enumerate)
  typedef struct udev_list_entry * (*udev_enumerate_get_list_entry_func)(struct udev_enumerate *);
  udev_enumerate_get_list_entry_func udev_enumerate_get_list_entry_ptr = NULL;
#define udev_enumerate_get_list_entry(udev_enumerate) (*udev_enumerate_get_list_entry_ptr)(udev_enumerate)
  typedef const char * (*udev_list_entry_get_name_func)(struct udev_list_entry *);
  udev_list_entry_get_name_func udev_list_entry_get_name_ptr = NULL;
#define udev_list_entry_get_name(list_entry) (*udev_list_entry_get_name_ptr)(list_entry)
  typedef struct udev_device * (*udev_device_new_from_syspath_func)(struct udev *, const char *);
  udev_device_new_from_syspath_func udev_device_new_from_syspath_ptr = NULL;
#define udev_device_new_from_syspath(udev, syspath) (*udev_device_new_from_syspath_ptr)(udev, syspath)
  typedef struct udev_device * (*udev_device_get_parent_with_subsystem_devtype_func)(struct udev_device *, const char *, const char *);
  udev_device_get_parent_with_subsystem_devtype_func udev_device_get_parent_with_subsystem_devtype_ptr = NULL;
#define udev_device_get_parent_with_subsystem_devtype(udev_device, subsystem, devtype) (*udev_device_get_parent_with_subsystem_devtype_ptr)(udev_device, subsystem, devtype)
  typedef const char * (*udev_device_get_sysattr_value_func)(struct udev_device *, const char *);
  udev_device_get_sysattr_value_func udev_device_get_sysattr_value_ptr = NULL;
#define udev_device_get_sysattr_value(udev_device, sysattr) (*udev_device_get_sysattr_value_ptr)(udev_device, sysattr)
  typedef const char * (*udev_device_get_devnode_func)(struct udev_device *);
  udev_device_get_devnode_func udev_device_get_devnode_ptr = NULL;
#define udev_device_get_devnode(udev_device) (*udev_device_get_devnode_ptr)(udev_device)
  typedef void (*udev_device_unref_func)(struct udev_device *);
  udev_device_unref_func udev_device_unref_ptr = NULL;
#define udev_device_unref(udev_device) (*udev_device_unref_ptr)(udev_device)
  typedef struct udev_list_entry * (*udev_list_entry_get_next_func)(struct udev_list_entry *);
  udev_list_entry_get_next_func udev_list_entry_get_next_ptr = NULL;
#define udev_list_entry_get_next(list_entry) (*udev_list_entry_get_next_ptr)(list_entry)
  typedef void (*udev_enumerate_unref_func)(struct udev_enumerate *);
  udev_enumerate_unref_func udev_enumerate_unref_ptr = NULL;
#define udev_enumerate_unref(udev_enumerate) (*udev_enumerate_unref_ptr)(udev_enumerate)
  typedef void (*udev_unref_func)(struct udev *);
  udev_unref_func udev_unref_ptr = NULL;
#define udev_unref(udev) (*udev_unref_ptr)(udev)

  if (
      ((udev_handle = load_dll("libudev.so.0")) == NULL &&
       (udev_handle = load_dll("libudev.so.1")) == NULL) ||
      (udev_new_ptr = (udev_new_func)load_symbol(udev_handle, "udev_new")) == NULL ||
      (udev_enumerate_new_ptr = (udev_enumerate_new_func)load_symbol(udev_handle, "udev_enumerate_new")) == NULL ||
      (udev_enumerate_add_match_subsystem_ptr = (udev_enumerate_add_match_subsystem_func)load_symbol(udev_handle, "udev_enumerate_add_match_subsystem")) == NULL ||
      (udev_enumerate_scan_devices_ptr = (udev_enumerate_scan_devices_func)load_symbol(udev_handle, "udev_enumerate_scan_devices")) == NULL ||
      (udev_enumerate_get_list_entry_ptr = (udev_enumerate_get_list_entry_func)load_symbol(udev_handle, "udev_enumerate_get_list_entry")) == NULL ||
      (udev_list_entry_get_name_ptr = (udev_list_entry_get_name_func)load_symbol(udev_handle, "udev_list_entry_get_name")) == NULL ||
      (udev_device_new_from_syspath_ptr = (udev_device_new_from_syspath_func)load_symbol(udev_handle, "udev_device_new_from_syspath")) == NULL ||
      (udev_device_get_parent_with_subsystem_devtype_ptr = (udev_device_get_parent_with_subsystem_devtype_func)load_symbol(udev_handle, "udev_device_get_parent_with_subsystem_devtype")) == NULL ||
      (udev_device_get_sysattr_value_ptr = (udev_device_get_sysattr_value_func)load_symbol(udev_handle, "udev_device_get_sysattr_value")) == NULL ||
      (udev_device_get_devnode_ptr = (udev_device_get_devnode_func)load_symbol(udev_handle, "udev_device_get_devnode")) == NULL ||
      (udev_device_unref_ptr = (udev_device_unref_func)load_symbol(udev_handle, "udev_device_unref")) == NULL ||
      (udev_list_entry_get_next_ptr = (udev_list_entry_get_next_func)load_symbol(udev_handle, "udev_list_entry_get_next")) == NULL ||
      (udev_enumerate_unref_ptr = (udev_enumerate_unref_func)load_symbol(udev_handle, "udev_enumerate_unref")) == NULL ||
      (udev_unref_ptr = (udev_unref_func)load_symbol(udev_handle, "udev_unref")) == NULL ||
      false)
    {
      logg->logError("Please specify the tty device with -d. E.g. -d /dev/ttyACM0.");
      handleException();
    }
#endif

  // initialize udev
  udev = udev_new();
  if (!udev) {
    logg->logError("Accessing the udev library failed, please verify the device is attached or specify the tty device on the command line to override auto detection");
    handleException();
  }

  // obtain a list of all tty devices
  enumerate = udev_enumerate_new(udev);
  udev_enumerate_add_match_subsystem(enumerate, "tty");
  udev_enumerate_scan_devices(enumerate);
  devices = udev_enumerate_get_list_entry(enumerate);

  // iterate over each device
  udev_list_entry_foreach(dev_list_entry, devices) {
    // get the tty device
    struct udev_device *dev = udev_device_new_from_syspath(udev, udev_list_entry_get_name(dev_list_entry));

    // get the usb tty parent device, which contains the vendor id and product id
    struct udev_device *parent = udev_device_get_parent_with_subsystem_devtype(dev, "usb", "usb_device");
    if (!parent) {
      continue;
    }
    const char *vid = udev_device_get_sysattr_value(parent,"idVendor");
    const char *pid = udev_device_get_sysattr_value(parent, "idProduct");

    // 1fc9/0003 is the prototype device; 0d28/0004 is the release device
    if (!(strcmp(vid, "1fc9") == 0 && strcmp(pid, "0003") == 0) && !(strcmp(vid, "0d28") == 0 && strcmp(pid, "0004") == 0)) {
      continue;
    }

    device_found = true;
    strncpy(comport, udev_device_get_devnode(dev), buffersize);
    comport[buffersize - 1] = 0; // strncpy does not guarantee a null-terminated string
    logg->logMessage("Device detected on %s", comport);
    udev_device_unref(parent);
    break;
  }

  // clean up
  udev_enumerate_unref(enumerate);
  udev_unref(udev);

  if (device_found == false) {
    logg->logError("Device could not be found, please verify the device is attached or specify the tty device on the command line to override auto detection");
    handleException();
  }
}

#elif defined(__linux__)

inline void EnergyProbe::autoDetectDevice_OS(char *comport, int buffersize) {
  (void)comport;
  (void)buffersize;
  logg->logError("Please specify the tty device with -d. E.g. -d /dev/ttyACM0.");
  handleException();
}

#elif defined(DARWIN)

inline void EnergyProbe::autoDetectDevice_OS(char *comport, int buffersize) {
  logg->logError("On OSX, please specify the tty device with -d. E.g. -d /dev/cu.usbmodem411");
  handleException();
}

#else

#error "Unknown OS -- check uname / Makefile"

#endif

char* EnergyProbe::autoDetectDevice() {
  char* comport = NULL;
  int buffersize = 1024; // arbitrarily large buffer size

  // purposefully do not free comport on error, the os does it for us
  comport = (char*)malloc(buffersize + 1);
  comport[0] = 0;

  autoDetectDevice_OS(comport, buffersize);
  return comport;
}
