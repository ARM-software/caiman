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

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(WIN32)
#include <windows.h>

#define tHANDLE    HANDLE
#define HOST_CDECL __cdecl
#define THREAD_CREATE(THREAD_ID, THREAD_FUNC) THREAD_ID = CreateThread(NULL, 0, (unsigned long (__stdcall *)(void *))THREAD_FUNC, NULL, 0, NULL)
#define THREAD_JOIN(THREAD_ID) WaitForSingleObject(THREAD_ID, INFINITE)

#define unlink _unlink

#else

// Linux or DARWIN
#include <unistd.h>
#include <pthread.h>

#define tHANDLE    pthread_t
#define HOST_CDECL
#define THREAD_CREATE(THREAD_ID, THREAD_FUNC) pthread_create(&THREAD_ID, NULL, THREAD_FUNC, NULL)
#define THREAD_JOIN(THREAD_ID) pthread_join(THREAD_ID, NULL)

#endif

#include "EnergyProbe.h"
#include "Fifo.h"
#include "Logging.h"
#include "NiDaq.h"
#include "OlySocket.h"
#include "OlyUtility.h"
#include "SessionData.h"

#define DEBUG false

#define DEFAULT_PORT 8081

// Commands from Streamline, from StreamlineSetup.h
enum {
  COMMAND_REQUEST_XML = 0,
  COMMAND_DELIVER_XML = 1,
  COMMAND_APC_START   = 2,
  COMMAND_APC_STOP    = 3,
  COMMAND_DISCONNECT  = 4,
  COMMAND_PING        = 5
};

// Responses to Streamline, from Sender.h
enum {
  RESPONSE_XML = 1,
  RESPONSE_APC_DATA = 3,
  RESPONSE_ACK = 4,
  RESPONSE_NAK = 5,
  RESPONSE_ERROR = 0xFF
};

struct cmdline_t {
  int port;
  char* path;
  char* device;
  bool isdaq;
  bool local;
};

volatile bool gQuit = false;
static bool waitingOnCommand = false;
static bool waitingOnConnection = false;
static OlySocket* sock = NULL;
static FILE * binfile = NULL;
static Fifo * fifo = NULL;
static sem_t senderSem, senderThreadStarted;
tHANDLE stopThreadID, senderThreadID;

static void HOST_CDECL sigintHandler(int sig) {
  static bool beenHere = false;

  (void)sig;
  if (beenHere) {
    logg->logMessage("Caiman is being forced to shut down.");
    exit(-1);
  }
  logg->logMessage("Caiman is shutting down.");
  beenHere = true;
  gQuit = true;
  if (waitingOnConnection) {
    exit(1);
  }
}

static void writeData(const char* data, uint32_t length, int type) {
  // Send data over the socket, sending the type and size first
  unsigned char header[5];
  header[0] = type;
  header[1] = (length >> 0) & 0xff;
  header[2] = (length >> 8) & 0xff;
  header[3] = (length >> 16) & 0xff;
  header[4] = (length >> 24) & 0xff;
  sock->send((char*)&header, sizeof(header));
  sock->send((const char*)data, length);
}

void handleException() {
  static int numExceptions = 0;
  if (numExceptions++ > 0) {
    // it is possible one of the below functions itself can cause an exception, thus allow only one exception
    logg->logMessage("Received multiple exceptions, terminating caiman");
    exit(1);
  }
  fprintf(stderr, "%s", logg->getLastError());

  if (sock) {
    // send the error, regardless of the command sent by Streamline
    writeData(logg->getLastError(), strlen(logg->getLastError()), RESPONSE_ERROR);

    // cannot close the socket before Streamline issues the command, so wait for the command before exiting
    if (waitingOnCommand) {
      char discard;
      sock->receiveNBytes(&discard, 1);
    }

    sock->closeSocket();
  }

  if (binfile) {
    fclose(binfile);
  }

  delete logg;
  exit(1);
}

static void* stopThread(void* pVoid) {
  (void)pVoid;
  logg->logMessage("Launch stop thread");
  while (!gQuit) {
    // This thread will stall until the APC_STOP or PING command is received over the socket or the socket is disconnected
    unsigned char header[5];
    const int result = sock->receiveNBytes((char*)&header, sizeof(header));
    const char type = header[0];
    const int length = (header[1] << 0) | (header[2] << 8) | (header[3] << 16) | (header[4] << 24);
    if (result > 0) {
      if ((type != COMMAND_APC_STOP) && (type != COMMAND_PING)) {
	logg->logMessage("INVESTIGATE: Received unknown command type %d", type);
      } else {
	// verify a length of zero
	if (length == 0) {
	  if (type == COMMAND_APC_STOP) {
	    logg->logMessage("Stop command received.");
	    gQuit = true;
	  } else {
	    // Ping is used to make sure gator is alive and requires an ACK as the response
	    logg->logMessage("Ping command received.");
	    writeData(NULL, 0, RESPONSE_ACK);
	  }
	} else {
	  logg->logMessage("INVESTIGATE: Received stop command but with length = %d", length);
	}
      }
    }
  }

  logg->logMessage("Exit stop thread");
  return 0;
}

static void* senderThread(void* pVoid) {
  int length = 1;
  (void)pVoid;

  sem_post(&senderThreadStarted);

  while (length > 0 && !gQuit) {
    sem_wait(&senderSem);
    char *data = fifo->read(&length);
    if (data != NULL) {
      writeData(data, length, RESPONSE_APC_DATA);
      fifo->release();
    }
  }

  logg->logMessage("Exit sender thread");
  return 0;
}

static void streamlineSetup(Device *const device) {
  bool ready = false;
  unsigned char header[5];
  char* data;
  int response;

  // Receive commands from Streamline (master)
  while (!ready) {
    // receive command over socket
    waitingOnCommand = true;

    // receive type and length
    response = sock->receiveNBytes((char*)&header, sizeof(header));

    // After receiving a single byte, we are no longer waiting on a command
    waitingOnCommand = false;

    if (response < 0) {
      logg->logError("Unexpected socket disconnect");
      handleException();
    }

    const char type = header[0];
    const int length = (header[1] << 0) | (header[2] << 8) | (header[3] << 16) | (header[4] << 24);

    // add artificial limit
    if ((length < 0) || length > 1024 * 1024) {
      logg->logError("Invalid length received, %d", length);
      handleException();
    }

    data = NULL;
    if (length > 0) {
      // allocate memory to contain the data
      data = (char*)calloc(length + 1, 1);
      if (data == NULL) {
	logg->logError("Unable to allocate memory for xml");
	handleException();
      }

      // receive data
      response = sock->receiveNBytes(data, length);
      if (response < 0) {
	logg->logError("Unexpected socket disconnect");
	handleException();
      }

      // null terminate the data for string parsing
      data[length] = 0;
    }

    // parse and handle data
    switch (type) {
    case COMMAND_REQUEST_XML: {
      // Assume this is a request for captured XML
      int xmlLength;
      char * xml = device->getXML(&xmlLength);
      writeData(xml, xmlLength, RESPONSE_XML);
      free(xml);
      break;
    }
    case COMMAND_DELIVER_XML:
      logg->logError("Deliver XML command not supported");
      handleException();
    case COMMAND_APC_START:
      logg->logMessage("Received apc start request");
      ready = true;
      break;
    case COMMAND_APC_STOP:
      logg->logError("Received apc stop request before apc start request");
      handleException();
      break;
    case COMMAND_DISCONNECT:
      // Clear error log so no text appears on console and exit
      logg->logMessage("Received disconnect command");
      logg->logError("");
      handleException();
      break;
    default:
      logg->logError("Unknown command type, %d", type);
      handleException();
    }

    free(data);
  }
}

static void handleMagicSequence() {
  char magic[32];
  char streamline[64] = {0};

  // Receive magic sequence - can wait forever
  while (strcmp("STREAMLINE", streamline) != 0) {
    if (sock->receiveString(streamline, sizeof(streamline)) == -1) {
      logg->logError("Socket disconnected");
      handleException();
    }
  }

  // Send magic sequence - must be done first, after which error messages can be sent
  snprintf(magic, 32, "CAIMAN %i\n", CAIMAN_VERSION);
  sock->send(magic, strlen(magic));

  // Magic sequence complete, now wait for a command from Streamline
  waitingOnCommand = true;
  logg->logMessage("Completed magic sequence");
}

#if defined(SUPPORT_DAQ)
#define DAQ_HELP "--daq \t\tUse a National Instruments DAQ unit to collect data\n"
#else
#define DAQ_HELP ""
#endif

static void printHelp(const char* const msg, const char* const version_string) {
  logg->logError("%s"
		 "%s\n"
		 "At least one channel must be specified, all other parameters are optional:\n"
		 "outputpath\tpath to store the apc data; default is current dir\n"
		 "-r <ch>:<r>\tfor channel ch use a resistance of r milliohm\n"
		 "-p <port>\tport number upon which the server listens; default is %d\n"
		 "-l\t\tenable local mode and disable communication with Streamline\n"
		 "%s"
		 "-d <device>\tdevice name, eg 'COM4', '/dev/ttyACM0', overrides auto detect\n"
		 "-v/--version\tversion information\n"
		 "-h/--help\tthis help page\n", msg, version_string, DEFAULT_PORT, DAQ_HELP);
  handleException();
}

static struct cmdline_t parseCommandLine(int argc, char** argv) {
  struct cmdline_t cmdline;
  char version_string[256];
  cmdline.port = DEFAULT_PORT;
  cmdline.path = NULL;
  cmdline.device = NULL;
  cmdline.isdaq = false;
  cmdline.local = false;

  if (CAIMAN_VERSION < PROTOCOL_DEV) {
    snprintf(version_string, sizeof(version_string) - 1, "Streamline caiman version %d (DS-5 v5.%d)", CAIMAN_VERSION, CAIMAN_VERSION);
  } else {
    snprintf(version_string, sizeof(version_string) - 1, "Streamline caiman development version %d", CAIMAN_VERSION);
  }
  // Verify version_string is null terminated as snprintf isn't guaranteed to do so
  version_string[sizeof(version_string) - 1] = '\0';
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "-?") == 0 || strcmp(argv[i], "--help") == 0) {
      printHelp("", version_string);
    } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
      logg->logError("%s", version_string);
      handleException();
    } else if (strcmp(argv[i], "-r") == 0) {
      if (++i == argc) {
	logg->logError("No value provided on command line after -r option");
	handleException();
      }
      char * endptr;
      const long channel = strtol(argv[i], &endptr, 10);
      if (*endptr != ':') {
	logg->logError("Value provided to -r is malformed");
	handleException();
      }
      const long value = strtol(endptr + 1, &endptr, 10);
      if ((*endptr != '\0') || (channel < 0) || (channel >= MAX_CHANNELS) || (value <= 0)) {
	logg->logError("Value provided to -r is malformed");
	handleException();
      }
      gSessionData.mResistors[channel] = value;
    } else if (strcmp(argv[i], "-p") == 0) {
      if (++i == argc) {
	logg->logError("No port number provided on command line after -p option");
	handleException();
      }
      cmdline.port = strtol(argv[i], NULL, 10);
    } else if (strcmp(argv[i], "-l") == 0) {
      cmdline.local = true;
    } else if (strcmp(argv[i], "-d") == 0) {
      if (++i == argc) {
	logg->logError("No device name provided on command line after -d option");
	handleException();
      }
      cmdline.device = argv[i];
    } else if (strcmp(argv[i], "--daq") == 0) {
#if defined(SUPPORT_DAQ)
      cmdline.isdaq = true;
#else
      logg->logError("The --daq option is not supported in this build of caiman.");
      handleException();
#endif
    } else if (strcmp(argv[i], "--no-print-messages") == 0) {
      // Disables writing debug messages to stdout
      logg->setPrintMessages(false);
    } else if (argv[i][0] == '-') {
      char buf[1024];
      snprintf(buf, sizeof(buf) - 1, "Unrecognized option '%s'\n", argv[i]);
      // Verify buffer is null terminated as snprintf isn't guaranteed to do so
      buf[sizeof(buf) - 1] = '\0';
      printHelp(buf, version_string);
    } else {
      // Assume it is a path
      cmdline.path = argv[i];
    }
  }

  return cmdline;
}

int HOST_CDECL main(int argc, char *argv[]) {
  signal(SIGINT, &sigintHandler);

  // Will be freed by the OS when Caiman terminates
  char* outputPath = (char*)malloc(CAIMAN_PATH_MAX + 1);  // MUST stay allocated
  char* binaryPath = (char*)malloc(CAIMAN_PATH_MAX + 1);

  logg = new Logging(DEBUG);  // Set up global thread-safe logging
  util = new OlyUtility();  // Set up global utility class

  // Parse the command line parameters
  struct cmdline_t cmdline = parseCommandLine(argc, argv);

  // Set default output path to this directory
  if (cmdline.path == NULL) {
    strncpy(outputPath, ".", CAIMAN_PATH_MAX);
  } else {
    strncpy(outputPath, cmdline.path, CAIMAN_PATH_MAX);
  }
  outputPath[CAIMAN_PATH_MAX - 1] = 0; // strncpy does not guarantee a null-terminated string

  // Ensure the path ends with a path separator
  int n = strlen(outputPath);
  if (outputPath[n-1] != '/' && outputPath[n-1] != '\\') {
    strncat(outputPath, "/", CAIMAN_PATH_MAX - n - 1);
  }

  // Set up warnings file
  snprintf(binaryPath, CAIMAN_PATH_MAX, "%swarnings.xml", outputPath);
  unlink(binaryPath);
  logg->SetWarningFile(binaryPath);

  // Create a string representing the path to the binary output file and open it
  snprintf(binaryPath, CAIMAN_PATH_MAX, "%s0000000000", outputPath);
  if (cmdline.local) {
    if ((binfile = fopen(binaryPath, "wb")) == 0) {
      logg->logError("Unable to open output file: %s0000000000\nPlease check write permissions on this file.", outputPath);
      handleException();
    }
  } else {
    if (sem_init(&senderSem, 0, 0) ||
	sem_init(&senderThreadStarted, 0, 0)) {
      logg->logError("sem_init() failed");
      handleException();
    }
    fifo = new Fifo(1<<15, 1<<20, &senderSem);
    THREAD_CREATE(senderThreadID, senderThread);
    if (!senderThreadID) {
      logg->logError("Failed to create sender thread");
      handleException();
    }
  }

  // Verify data
  gSessionData.compileData();

  Device *device;
  if (cmdline.isdaq) {
#if defined(SUPPORT_DAQ)
    device = new NiDaq(outputPath, binfile, fifo);
#else
    // Intentionally redundant: CLI blocks isdaq if !SUPPORT_DAQ
    logg->logError("National Instruments DAQ is not supported in this build.");
    handleException();
#endif
  } else {
    device = new EnergyProbe(outputPath, binfile, fifo);
  }

  device->prepareChannels();

  // Create a socket as long as local was not specified
  if (!cmdline.local) {
    waitingOnConnection = true;
    logg->logMessage("Waiting on connection...");
    OlyServerSocket server(cmdline.port);
    sock = new OlySocket(server.acceptConnection());
    server.closeServerSocket();
    waitingOnConnection = false;
    handleMagicSequence();
  }

  if (sock) {
    // Wait for start command from Streamline
    streamlineSetup(device);

    // Create stop thread
    THREAD_CREATE(stopThreadID, stopThread);
    if (!stopThreadID) {
      logg->logError("Failed to create stop thread");
      handleException();
    }

    // Wait until thread has started
    sem_wait(&senderThreadStarted);
  } else {
    device->writeXML();
  }

  device->init(cmdline.device);

  // Start the device
  device->start();

  // Get the data
  while (!gQuit) {
    device->processBuffer();  // May (now) block thread for up to 1s (NiDaq)
  }
  logg->logMessage("Get data loop finished; caiman is shutting down");

  device->stop();

  // Shutting down the connection should break the stop thread which is stalling on the socket recv() function
  if (sock) {
    fifo->write(0);
    THREAD_JOIN(senderThreadID);
    sock->shutdownConnection();
    THREAD_JOIN(stopThreadID);
  }

  if (binfile) {
    fclose(binfile);
  }
  delete device;
  delete sock;
  delete logg;
  delete util;

  return 0;
}
