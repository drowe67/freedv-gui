#include <stdlib.h>
#include <string.h>
#include <assert.h>

extern "C" {
    #include "debug_alloc.h"
}

#include "o1heap.h"
#include "codec2_alloc.h"

static thread_local void* Heap_ = NULL;
static thread_local O1HeapInstance* Instance_ = NULL;

void codec2_initialize_realtime(size_t heapSize)
{
#if defined(WIN32)
    Heap_ = (void*)_aligned_malloc(heapSize, O1HEAP_ALIGNMENT);
#else
    Heap_ = (void*)aligned_alloc(O1HEAP_ALIGNMENT, heapSize);
#endif // defined(WIN32)
    assert(Heap_ != NULL);

    memset(Heap_, 0, heapSize);

    Instance_ = o1heapInit(Heap_, heapSize);
    assert(Instance_ != NULL);
}

void codec2_disable_realtime()
{
#if defined(WIN32)
    _aligned_free(Heap_);
#else
    free(Heap_);
#endif // defined(WIN32)
    Heap_ = NULL;
    Instance_ = NULL; // no need to deallocate per o1heap docs
}

void *codec2_malloc(size_t size)
{
    if (Instance_ == NULL) return malloc(size);
    else return o1heapAllocate(Instance_, size);
}

void *codec2_calloc(size_t nmemb, size_t size)
{
    void* ptr = codec2_malloc(nmemb * size);
    assert(ptr != NULL);

    memset(ptr, 0, nmemb * size);
    return ptr;
}

void codec2_free(void *ptr)
{
    if (Instance_ == NULL) free(ptr);
    else return o1heapFree(Instance_, ptr);
}
