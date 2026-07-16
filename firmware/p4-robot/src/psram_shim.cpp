// psram_shim.cpp — Intercept heap allocations to add PSRAM fallback for DMA
// The arduino-esp32 v3.3.10 platform libs lack CONFIG_SPIRAM_USE_CAPS_ALLOC,
// so heap_caps_*(large, MALLOC_CAP_DMA) fails without MALLOC_CAP_SPIRAM.
// We remove the DMA cap requirement on fallback — PSRAM on P4 is physically
// DMA-capable via GDMA, the allocator just doesn't tag it as such.
#include <esp_heap_caps.h>
#include <cstdint>
#include <cstring>

extern "C" {

// Helper: strip MALLOC_CAP_DMA from caps for SPIRAM fallback
static inline uint32_t dma_fallback_caps(uint32_t caps) {
    return (caps & ~((uint32_t)MALLOC_CAP_DMA)) | MALLOC_CAP_SPIRAM;
}

void *__real_heap_caps_malloc(size_t size, uint32_t caps);
void *__wrap_heap_caps_malloc(size_t size, uint32_t caps) {
    void *ptr = __real_heap_caps_malloc(size, caps);
    if (ptr) return ptr;
    if ((caps & MALLOC_CAP_DMA) && !(caps & MALLOC_CAP_SPIRAM) && size >= 512 * 1024) {
        ptr = __real_heap_caps_malloc(size, dma_fallback_caps(caps));
    }
    return ptr;
}

void *__real_heap_caps_calloc(size_t n, size_t size, uint32_t caps);
void *__wrap_heap_caps_calloc(size_t n, size_t size, uint32_t caps) {
    void *ptr = __real_heap_caps_calloc(n, size, caps);
    if (ptr) return ptr;
    if ((caps & MALLOC_CAP_DMA) && !(caps & MALLOC_CAP_SPIRAM)) {
        size_t total = n * size;
        if (total >= 512 * 1024) {
            ptr = __real_heap_caps_calloc(n, size, dma_fallback_caps(caps));
        }
    }
    return ptr;
}

void *__real_heap_caps_realloc(void *p, size_t size, uint32_t caps);
void *__wrap_heap_caps_realloc(void *p, size_t size, uint32_t caps) {
    void *ptr = __real_heap_caps_realloc(p, size, caps);
    if (ptr) return ptr;
    if ((caps & MALLOC_CAP_DMA) && !(caps & MALLOC_CAP_SPIRAM) && size >= 512 * 1024) {
        ptr = __real_heap_caps_realloc(p, size, dma_fallback_caps(caps));
    }
    return ptr;
}

// Dummy init to verify this file is compiled & linked
__attribute__((constructor)) static void psram_shim_init(void) {
    // Verify wrappers exist at link time
    volatile void *check = (void*)&__wrap_heap_caps_calloc;
    (void)check;
}

}
