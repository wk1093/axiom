#include "ax_vec.h"
#include <stdlib.h>
#include <string.h>

void* ax_vecNewImpl(size_t element_size) {
    size_t initial_cap = 4;

    size_t total_size = sizeof(AxVecHeader) + (element_size * initial_cap);
    AxVecHeader* h = malloc(total_size);
    if (!h) return NULL;

    h->capacity = initial_cap;
    h->size = 0;
    h->element_size = element_size;

    return (void*)(h + 1);
}

void ax_vecFree(void* v) {
    if (v) {
        free(ax_vecHdr(v));
    }
}

void* ax_vecPushImpl(void* v, const void* item) {
    if (!v) return NULL;

    AxVecHeader* h = ax_vecHdr(v);

    if (h->size >= h->capacity) {
        size_t new_cap = h->capacity * 2;
        size_t total_size = sizeof(AxVecHeader) + (h->element_size * new_cap);
        h = realloc(h, total_size);
        if (!h) return NULL;
        h->capacity = new_cap;
        v = (void*)(h+1);
    }

    void* target = (char*)v + (h->size * h->element_size);
    memcpy(target, item, h->element_size);
    h->size++;

    return v;
}

