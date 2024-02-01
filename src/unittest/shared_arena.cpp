/// \file shared_arena.cpp
///  
/// Unit tests for shared memory arena
///

#include <iostream>
#include <vector>

#include "../config/allocator_config.hpp"

#include "catch.hpp"

namespace vg {


namespace unittest {
using namespace std;
    
TEST_CASE("Allocation in a shared memory arena works", "[arena]") {

    // The arena has a min size of a few megabytes.
    const size_t ARENA_SIZE = 4 * 1024 * 1024;
    char arena_mem[ARENA_SIZE];

    bool opened = AllocatorConfig::set_arena_area(arena_mem, ARENA_SIZE);

    void* allocated = malloc(10);
    if (allocated) {
        free(allocated);
    }

    bool closed = AllocatorConfig::set_arena_area(nullptr, 0);

    REQUIRE(opened == true);
    REQUIRE(closed == true);

    REQUIRE((intptr_t) allocated >= (intptr_t) arena_mem);
    REQUIRE((intptr_t) allocated < (intptr_t) arena_mem + ARENA_SIZE);
    
}
}
}
        

