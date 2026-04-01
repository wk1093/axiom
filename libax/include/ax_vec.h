#ifndef AX_VEC_H
#define AX_VEC_H

#include <stddef.h>
#include <assert.h>

typedef struct {
    size_t capacity;
    size_t size;
    size_t element_size;
    size_t _padding;
} AxVecHeader;

_Static_assert(sizeof(AxVecHeader) == 32, "AxVecHeader must be exactly 32 bytes for 16-byte alignment");

#define ax_vecHdr(v) ((AxVecHeader*)(v)-1)

void* ax_vecNewImpl(size_t elementSize);
void ax_vecFree(void* v);
void* ax_vecPushImpl(void* v, const void* item);

void* ax_vecResizeImpl(void* v, size_t new_size);

void ax_vecRemoveAt(void* v, size_t index);

#define ax_vecNew(type) ((type*)ax_vecNewImpl(sizeof(type)))

#define ax_vecPush(v, item) { \
    typeof(item) _item = (item); \
    (v) = (typeof(v))ax_vecPushImpl((v), &_item); \
}
#define ax_vecSize(v) ((v) ? ax_vecHdr(v)->size : 0)

#define ax_vecResize(v, new_size) ((v) = (typeof(v))ax_vecResizeImpl((v), (new_size)))

#endif
