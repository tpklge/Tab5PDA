#pragma once
#include <stddef.h>
#ifdef ARDUINO
#include <esp_heap_caps.h>
static inline void *tab_lv_malloc(size_t n) {
    return heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}
static inline void *tab_lv_realloc(void *p, size_t n) {
    return heap_caps_realloc(p, n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}
static inline void tab_lv_free(void *p) {
    heap_caps_free(p);
}
#else
#include <stdlib.h>
static inline void *tab_lv_malloc(size_t n) {
    return malloc(n);
}
static inline void *tab_lv_realloc(void *p, size_t n) {
    return realloc(p, n);
}
static inline void tab_lv_free(void *p) {
    free(p);
}
#endif
