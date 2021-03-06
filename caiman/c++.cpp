/**
 * Copyright (C) 2011-2020 by Arm Limited. All rights reserved.
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

#ifdef __GNUC__

#include <stdio.h>
#include <stdlib.h>

void operator delete(void *ptr)
{
    if (ptr != NULL) {
        free(ptr);
    }
}

void operator delete[](void *ptr)
{
    operator delete(ptr);
}

void *operator new(size_t size)
{
    void *ptr = malloc(size == 0 ? 1 : size);
    if (ptr == NULL) {
        abort();
    }
    return ptr;
}

void *operator new[](size_t size)
{
    return operator new(size);
}

extern "C" void __cxa_pure_virtual()
{
    printf("pure virtual method called\n");
    abort();
}

#endif
