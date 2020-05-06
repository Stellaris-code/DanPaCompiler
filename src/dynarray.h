/*
dynarray.h

Copyright (c) 16 Yann BOUCHER (yann)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/
#ifndef DYNARRAY_H
#define DYNARRAY_H

#include "alloc.h"

#define DYNARRAY_PRE_ALLOCATE 0

#define DYNARRAY(type) \
    struct { \
        int size, capacity; \
        type* ptr; \
    }

#define DYNARRAY_INIT(array, default_capacity) \
    do { \
    (array).size = 0; \
    (array).capacity = (default_capacity) * DYNARRAY_PRE_ALLOCATE; \
    if ((array).capacity) \
        (array).ptr = danpa_alloc((array).capacity * sizeof(*(array).ptr)); \
    else \
        (array).ptr = NULL; \
    } while (0)

#define DYNARRAY_RESIZE(array, target_size) \
    do { \
    (array).size = target_size; \
    while ((array).size >= (array).capacity) {\
        if ((array).capacity == 0) \
            (array).capacity = 1; \
        (array).capacity *= 2;} \
        (array).ptr = danpa_realloc((array).ptr, sizeof(*(array).ptr)*(array).capacity);\
    } while (0)

#define DYNARRAY_ADD(array, ...) \
    do { \
    ++(array).size; \
    if ((array).size >= (array).capacity) {\
        if ((array).capacity == 0) \
            (array).capacity = 1; \
        (array).capacity *= 2; \
        (array).ptr = danpa_realloc((array).ptr, sizeof(*(array).ptr)*(array).capacity);}\
     typeof(*(array).ptr) tmp = __VA_ARGS__; \
    (array).ptr[(array).size-1] = tmp; \
    } while (0)

#define DYNARRAY_POP(array) \
    do { \
    --(array).size; \
    } while (0)

#define DYNARRAY_BACK(array) \
    ((array).ptr[(array).size-1])

#endif // DYNARRAY_H
