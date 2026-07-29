#ifndef ARRAY_H_
#define ARRAY_H_

#include <stdlib.h>
#include <assert.h>

#include "util.h"

#define DA_INIT_CAP 256

#define ARRAY_LEN(array) (sizeof(array)/sizeof(array[0]))
#define ARRAY_GET(array, index) \
    (assert((size_t)index < ARRAY_LEN(array)), array[(size_t)index])

#define da_reserve(da, expected_capacity)                                                  \
    do {                                                                                   \
        if ((expected_capacity) > (da)->capacity) {                                        \
            if ((da)->capacity == 0) {                                                     \
                (da)->capacity = DA_INIT_CAP;                                              \
            }                                                                              \
            while ((expected_capacity) > (da)->capacity) {                                 \
                (da)->capacity *= 2;                                                       \
            }                                                                              \
            (da)->items = DELCTYPE((da)->items)realloc((da)->items, (da)->capacity * sizeof(*(da)->items)); \
            assert((da)->items != NULL && "memory not enough");                            \
        }                                                                                  \
    } while (0)

#define da_append(da, item)                  \
    do {                                     \
        da_reserve((da), (da)->count + 1);   \
        (da)->items[(da)->count++] = (item); \
    } while (0)

#define da_free(da) free((da).items)

#define da_sort(da, cmp)                                                  \
    do {                                                                  \
        if ((da)->count > 0)                                              \
            qsort((da)->items, (da)->count, sizeof(*(da)->items), (cmp)); \
    } while (0)

// Append several items to a dynamic array
#define da_appends(da, nitems, items_count)                                              \
    do {                                                                                \
        da_reserve((da), (da)->count + (items_count));                                  \
        memcpy((da)->items + (da)->count, (nitems), (items_count)*sizeof(*(da)->items)); \
        (da)->count += (items_count);                                                   \
    } while (0)

#define da_resize(da, size)     \
    do {                        \
        da_reserve((da), size); \
        (da)->count = (size);   \
    } while (0)

#define da_pop(da) (da)->items[(assert((da)->count > 0), --(da)->count)]
#define da_first(da) (da)->items[(assert((da)->count > 0), 0)]
#define da_last(da) (da)->items[(assert((da)->count > 0), (da)->count-1)]
#define da_remove_unordered(da, i)                   \
    do {                                             \
        size_t j = (i);                              \
        assert(j < (da)->count);                     \
        (da)->items[j] = (da)->items[--(da)->count]; \
    } while(0)

#define da_foreach(Type, it, da) for (Type *it = (da)->items; it < (da)->items + (da)->count; ++it)

// The Fixed Array append. `items` fields must be a fixed size array. Its size determines the capacity.
#define fa_append(fa, item) \
    (assert((fa)->count < ARRAY_LEN((fa)->items)), \
     (fa)->items[(fa)->count++] = (item))

#endif // ARRAY_H_
