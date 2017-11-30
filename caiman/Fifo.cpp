/**
 * Copyright (C) Arm Limited 2010-2016. All rights reserved.
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

#include "Fifo.h"

#include <stdlib.h>

#include "Logging.h"

// bufferSize is the amount of data to be filled
// singleBufferSize is the maximum size that may be filled during a single write
// (bufferSize + singleBufferSize) will be allocated
Fifo::Fifo(int singleBufferSize, int bufferSize, sem_t* readerSem)
{
    mWrite = mRead = mReadCommit = mRaggedEnd = 0;
    mWrapThreshold = bufferSize;
    mSingleBufferSize = singleBufferSize;
    mReaderSem = readerSem;
    mBuffer = (char*) malloc(bufferSize + singleBufferSize);
    mEnd = false;

    if (mBuffer == NULL) {
        logg.logError("failed to allocate %d bytes", bufferSize + singleBufferSize);
        handleException();
    }

    if (sem_init(&mWaitForSpaceSem, 0, 0)) {
        logg.logError("sem_init() failed");
        handleException();
    }
}

Fifo::~Fifo()
{
    free(mBuffer);
    sem_destroy(&mWaitForSpaceSem);
}

int Fifo::numBytesFilled() const
{
    return mWrite - mRead + mRaggedEnd;
}

char* Fifo::start() const
{
    return mBuffer;
}

bool Fifo::isEmpty() const
{
    return mRead == mWrite && mRaggedEnd == 0;
}

bool Fifo::isFull() const
{
    return willFill(0);
}

// Determines if the buffer will fill assuming 'additional' bytes will be added to the buffer
// 'full' means there is less than singleBufferSize bytes available contiguously; it does not mean there are zero bytes available
bool Fifo::willFill(int additional) const
{
    if (mWrite > mRead) {
        if (numBytesFilled() + additional < mWrapThreshold) {
            return false;
        }
    }
    else {
        if (numBytesFilled() + additional < mWrapThreshold - mSingleBufferSize) {
            return false;
        }
    }
    return true;
}

// This function will stall until contiguous singleBufferSize bytes are available
char* Fifo::write(int length)
{
    if (length <= 0) {
        length = 0;
        mEnd = true;
    }

    // update the write pointer
    mWrite += length;

    // handle the wrap-around
    if (mWrite >= mWrapThreshold) {
        mRaggedEnd = mWrite;
        mWrite = 0;
    }

    // send a notification that data is ready
    sem_post(mReaderSem);

    // wait for space
    while (isFull()) {
        sem_wait(&mWaitForSpaceSem);
    }

    return &mBuffer[mWrite];
}

void Fifo::release()
{
    // update the read pointer now that the data has been handled
    mRead = mReadCommit;

    // handle the wrap-around
    if (mRead >= mWrapThreshold) {
        mRaggedEnd = mRead = mReadCommit = 0;
    }

    // send a notification that data is free (space is available)
    sem_post(&mWaitForSpaceSem);
}

// This function will return null if no data is available
char* Fifo::read(int * const length)
{
    // wait for data
    if (isEmpty() && !mEnd) {
        return NULL;
    }

    // obtain the length
    do {
        mReadCommit = mRaggedEnd ? mRaggedEnd : mWrite;
        *length = mReadCommit - mRead;
    } while (*length < 0); // plugs race condition without using semaphores

    return &mBuffer[mRead];
}
