#include <cstring>
#include <new>
#include "tools/aligned_memalloc.h"

void *aligned_malloc(size_t size) {
  return operator new(size, (std::align_val_t)tsl::CACHELINE);
}

void aligned_free(void *ptr){
  operator delete(ptr, (std::align_val_t)tsl::CACHELINE);
}

void *aligned_calloc(size_t size){
  void *ptr = aligned_malloc(size);
  memset(ptr, 0, size);
  return ptr;
}
