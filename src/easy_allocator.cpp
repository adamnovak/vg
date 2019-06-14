/**
 * \file easy_allocator.cpp
 *
 * Defines a simple malloc implementation that wants to be fast.
 *
 * Replaces the functions required by https://www.gnu.org/software/libc/manual/html_node/Replacing-malloc.html
 */

#include <cstddef>
#include <cstdlib>
#include <cerrno>
#include <cstring>

#include <algorithm>

#include <unistd.h>

/**
 * Each allocation we do has one of these headers to represent it. The header
 * ends immediately before the allocation starts, and contains the number of
 * bytes before and after the header that belong to the block. This flexibility
 * lets us allocate aligned allocations from not-necessarily-alligned blocks.
 */
struct AllocationHeader {
    /// How many bytes before the header belong to the block
    size_t bytes_before;
    /// How many bytes after the header belong to the block
    size_t bytes_after;
};

/**
 * Each block in the free list has this storead at its beginning.
 * Size is tracked by which free list a block is in.
 */
struct FreeBlock {
    /// Where's the next block in the free list?
    /// Null if this block is the last one.
    FreeBlock* next;
};

/// Per-thread free lists. One for each power of 2 of the block containing the header and the item.
/// First few may never be used. Starts out all null.
static thread_local FreeBlock* thread_free_list[64] = {};

/// Super simple thread-local fake heap
const size_t THREAD_HEAP_BYTES = 1024 * 1024 * 1024;
static thread_local char thread_heap[THREAD_HEAP_BYTES];
static thread_local size_t thread_heap_offset = 0;

/// Define a way to get more memory.
/// Returns nullptr if the OS is out of memory.
/// May up bytes if more memory than requested gets gotten.
void* get_more_memory(size_t& bytes) {
    // For now do an easy thread-local way.
    // For a real implementation this might have to explicitly or implicitly lock.
    
    if (thread_heap_offset + bytes > THREAD_HEAP_BYTES) {
        // No more memory
        return nullptr;
    }
    
    void* got = (void*) &thread_heap[thread_heap_offset];
    thread_heap_offset += bytes;
    return got;
}


/// Each thread has a linked list of allocation headers

extern "C" {

/// Allocate this many bytes or return null
void* malloc(size_t size) {
    
    // Decide how much space we really need.
    size_t size_with_header = size + sizeof(AllocationHeader);

    void* got = get_more_memory(size_with_header);
    
    if (got == nullptr) {
        // No more memory available
        return nullptr;
    }
    
    // Store info about block in header, including size we actually got.
    AllocationHeader& header = *((AllocationHeader*) got);
    header.bytes_before = 0;
    header.bytes_after = size_with_header - sizeof(AllocationHeader);
    
    // Leave room for the header and return the actual usable block
    return (void*) (((AllocationHeader*) got) + 1);
}

/// Free an allocation from any of the allocation functions.
void free(void* ptr) {
}

/// Allocate nmemb items of the given size, and zero them.
/// Either may be 0.
void* calloc(size_t nmemb, size_t size) {
    void* got = malloc(nmemb * size);
    if (got == nullptr) {
        return nullptr;
    }
    
    // Zero it out
    memset(got, 0, nmemb * size);
    return got;
}

/// Allocate given a null pointer, or resize if given an allocated pointer from malloc or calloc.
/// If data can't be resized in place, copies data to a new allocation and frees the old one.
void* realloc(void* ptr, size_t size) {
    
    if (ptr == nullptr) {
        return malloc(size);
    }
    
    // Find the block's header
    AllocationHeader& header = *((AllocationHeader*) ptr - 1);
    
    // Trivial implementation: allocate, copy always, free
    size_t old_size = header.bytes_after;
    void* new_location = malloc(size);
    memcpy(new_location, ptr, std::min(size, old_size));
    free(ptr);
    
    return new_location;
}

/// Allocate memory aligned to alignment, which is a power of 2 but maybe not a multiple of sizeof(void*)
void* memalign(size_t alignment, size_t size) {
    
    // How big do we need to have to be able to guarantee correct alignment?
    size_t sufficiently_big = size + alignment;
    // Get it
    void* allocated = malloc(sufficiently_big);
    
    if (allocated == nullptr) {
        // No memory left
        return nullptr;
    }
    
    size_t address = (size_t) allocated;
    
    if (address % alignment == 0) {
        // Already aligned
        return allocated;
    }
    
    // Otherwise, fix it up. Work out the address we will ship out
    size_t aligned_address = address - (address % alignment) + alignment;
    void* new_handle_address = (void*) aligned_address;
    
    // Work out the new header we will have there.
    AllocationHeader& new_header = *((AllocationHeader*) new_handle_address - 1);
    
    // Save the old header to stack
    AllocationHeader old_header = *(((AllocationHeader*) allocated) - 1);
    
    // Account for the shift and write the new header.
    new_header.bytes_before = old_header.bytes_before + (aligned_address - address);
    new_header.bytes_after = old_header.bytes_after - (aligned_address - address);
    
    // Send out the address right after the now-shifted header.
    return new_handle_address;
}

/// Allocate memory aligned to alignment, which is a power of 2 and a multiple of sizeof(void*)
/// Store result at memptr. Return 0 on success, or ENOMEM or EINVAL on failure.
int posix_memalign(void** memptr, size_t alignment, size_t size) {
    
    *memptr = memalign(alignment, size);
    
    if (*memptr == nullptr) {
        return ENOMEM;
    }
    
    return 0;
    
}


/// Allocate memory aligned to alignment, which is a power of 2 but maybe not a
/// multiple of sizeof(void*), and where size is a multiple of alignment
void* aligned_alloc(size_t alignment, size_t size) {
    return memalign(alignment, size);
}

/// Allocate memory aligned to page size.
/// Same as memalign(sysconf(_SC_PAGESIZE),size)
void* valloc(size_t size) {
    return memalign(sysconf(_SC_PAGESIZE), size);
}

/// Same as valloc, but rounds size up to next multiple of page size
void* pvalloc(size_t size) {
    size_t page_size = sysconf(_SC_PAGESIZE);
    
    if (size % page_size != 0) {
        // Round up to next multiple of page size
        size = size - (size % page_size) + page_size;
    }
    
    return valloc(size);
}
    
}
