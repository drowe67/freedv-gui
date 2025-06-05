#ifndef CODEC2_ALLOC_H
#define CODEC2_ALLOC_H

#ifdef __cplusplus
extern "C" {
#endif

void codec2_initialize_realtime(size_t heapSize);
void codec2_disable_realtime();

#ifdef __cplusplus
}
#endif

#endif // CODEC2_ALLOC_H
