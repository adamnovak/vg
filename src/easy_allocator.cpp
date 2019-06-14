/**
 * \file easy_allocator.cpp
 *
 * Defines a simple malloc implementation that wants to be fast.
 *
 * Replaces the functions required by https://www.gnu.org/software/libc/manual/html_node/Replacing-malloc.html
 */

#include <cstddef>
#include <cstdlib>

extern "C" {

/// Allocate this many bytes or return null
void* malloc(size_t size) {
    return nullptr;
}

/// Free an allocation from any of the allocation functions.
void free(void* ptr) {
}

/// Allocate nmemb items of the given size, and zero them.
/// Either may be 0.
void* calloc(size_t nmemb, size_t size) {
    return nullptr;
}

/// Allocate given a null pointer, or resize if given an allocated pointer.
/// If data can't be resized in place, copies data to a new allocation and frees the old one.
void* realloc(void* ptr, size_t size) {
    return nullptr;
}

/// Allocate memory aligned to alignment, which is a power of 2 and a multiple of sizeof(void*)
/// Storew result at memptr. Return 0 on success, or ENOMEM or EINVAL on failure.
int posix_memalign(void** memptr, size_t alignment, size_t size) {
    memptr = nullptr;
    return ENOMEM;
}


/// Allocate memory aligned to alignment, which is a power of 2 but maybe not a
/// multiple of sizeof(void*), and wherer size is a multiple of alignment
void* aligned_alloc(size_t alignment, size_t size) {
    return nullptr;
}

/// Allocate memory aligned to page size.
/// Same as memalign(sysconf(_SC_PAGESIZE),size)
void* valloc(size_t size) {
    return nullptr;
}

/// Allocate memory aligned to alignment, which is a power of 2 but maybe not a multiple of sizeof(void*)
void* memalign(size_t alignment, size_t size) {
    return nullptr;
}

/// Same as valloc, but rounds size up to next multiple of page size
void* pvalloc(size_t size) {
    return nullptr;
}
    
}
