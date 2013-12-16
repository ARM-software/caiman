/**
 * Copyright (C) ARM Limited 2010-2013. All rights reserved.
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

#ifndef __OLY_SOCKET_H__
#define __OLY_SOCKET_H__

#include <string.h>

class OlySocket {
public:
  OlySocket(int port, bool multipleConnections = false);
  OlySocket(int port, char* hostname);
  ~OlySocket();
  int acceptConnection();
  void closeSocket();
  void closeServerSocket();
  void shutdownConnection();
  void send(char* buffer, int size);
  void sendString(const char* string) {send((char*)string, strlen(string));}
  int receive(char* buffer, int size);
  int receiveNBytes(char* buffer, int size);
  int receiveString(char* buffer, int size);
  int getSocketID() {return mSocketID;}
private:
  int mSocketID, mFDServer;
  void createClientSocket(char* hostname, int port);
  void createSingleServerConnection(int port);
  void createServerSocket(int port);
};

#endif //__OLY_SOCKET_H__
