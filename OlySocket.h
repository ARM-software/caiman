/**
 * Copyright (C) ARM Limited 2010-2014. All rights reserved.
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

class OlySocket {
public:
  OlySocket(int port, const char* hostname);
  OlySocket(int socketID);
#ifndef WIN32
  OlySocket(const char* path);
#endif
  ~OlySocket();

  void closeSocket();
  void shutdownConnection();
  void send(const char* buffer, int size);
  int receive(char* buffer, int size);
  int receiveNBytes(char* buffer, int size);
  int receiveString(char* buffer, int size);

  bool isValid() const { return mSocketID >= 0; }

private:
  int mSocketID;

  void createClientSocket(const char* hostname, int port);
};

class OlyServerSocket {
public:
  OlyServerSocket(int port);
#ifndef WIN32
  OlyServerSocket(const char* path);
#endif
  ~OlyServerSocket();

  int acceptConnection();
  void closeServerSocket();

private:
  int mFDServer;

  void createServerSocket(int port);
};

#endif //__OLY_SOCKET_H__
