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

    const size_t ARENA_SIZE = 1024;
    char arena_mem[ARENA_SIZE];

    bool opened = AllocatorConfig::set_arena_area(arena_mem, ARENA_SIZE);

    std::string the_string = "This string will go on the arena";

    bool closed = AllocatorConfig::set_arena_area(nullptr, 0);

    REQUIRE(opened == true);
    REQUIRE(closed == true);

    intptr_t string_loc = (intptr_t) the_string.c_str();

    REQUIRE(string_loc >= (intptr_t) arena_mem);
    REQUIRE(string_loc < (intptr_t) arena_mem + ARENA_SIZE);
    
}
}
}
        

