/**
 * Copyright (C) ARM Limited 2010-2016. All rights reserved.
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

#ifndef __FIFO_H__
#define __FIFO_H__

#ifdef WIN32
#include <windows.h>
#define sem_t HANDLE
#define sem_init(sem, pshared, value) ((*(sem) = CreateSemaphore(NULL, value, LONG_MAX, NULL)) == NULL)
#define sem_wait(sem) WaitForSingleObject(*(sem), INFINITE)
#define sem_post(sem) ReleaseSemaphore(*(sem), 1, NULL)
#define sem_destroy(sem) CloseHandle(*(sem))
#else
#include <semaphore.h>
#endif

class Fifo
{
public:
    Fifo(int singleBufferSize, int totalBufferSize, sem_t* readerSem);
    ~Fifo();
    int numBytesFilled() const;
    bool isEmpty() const;
    bool isFull() const;
    bool willFill(int additional) const;
    char* start() const;
    char* write(int length);
    void release();
    char* read(int * const length);

private:
    int mSingleBufferSize, mWrite, mRead, mReadCommit, mRaggedEnd, mWrapThreshold;
    sem_t mWaitForSpaceSem;
    sem_t* mReaderSem;
    char* mBuffer;
    bool mEnd;

    // Intentionally unimplemented
    Fifo(const Fifo &);
    Fifo &operator=(const Fifo &);
};

#endif //__FIFO_H__
