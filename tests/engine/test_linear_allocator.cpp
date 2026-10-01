#include <catch2/catch_test_macros.hpp>
#include "engine/linear_allocator.h"

#include <cstdint>
#include <new>

using namespace engine;

TEST_CASE("linear_allocator: basic allocation", "[allocator]") {
    LinearAllocator alloc(4096);
    void* p = alloc.allocate(16);
    REQUIRE(p != nullptr);
    REQUIRE(alloc.bytes_used() >= 16);
}

TEST_CASE("linear_allocator: sequential allocations do not overlap", "[allocator]") {
    LinearAllocator alloc(4096);
    void* p1 = alloc.allocate(16);
    void* p2 = alloc.allocate(16);

    REQUIRE(p1 != p2);
    // p2 should come after p1.
    REQUIRE(static_cast<std::byte*>(p2) >= static_cast<std::byte*>(p1) + 16);
}

TEST_CASE("linear_allocator: alignment is respected", "[allocator]") {
    LinearAllocator alloc(4096);

    void* p1 = alloc.allocate(1, 1);      // misalign the offset
    void* p2 = alloc.allocate(8, 8);
    void* p3 = alloc.allocate(8, 16);
    void* p4 = alloc.allocate(8, 64);

    REQUIRE(reinterpret_cast<std::uintptr_t>(p2) % 8 == 0);
    REQUIRE(reinterpret_cast<std::uintptr_t>(p3) % 16 == 0);
    REQUIRE(reinterpret_cast<std::uintptr_t>(p4) % 64 == 0);
    (void)p1;
}

TEST_CASE("linear_allocator: reset makes the block reusable", "[allocator]") {
    LinearAllocator alloc(4096);
    alloc.allocate(1024);
    std::size_t used_before = alloc.bytes_used();
    REQUIRE(used_before >= 1024);

    alloc.reset();
    REQUIRE(alloc.bytes_used() == 0);
    REQUIRE(alloc.bytes_reserved() >= 4096);

    // Can allocate again.
    void* p = alloc.allocate(1024);
    REQUIRE(p != nullptr);
}

TEST_CASE("linear_allocator: alloc<T> constructs", "[allocator]") {
    struct Point { int x, y; };
    LinearAllocator alloc(4096);

    Point* p = alloc.alloc<Point>();
    p->x = 1;
    p->y = 2;

    REQUIRE(p->x == 1);
    REQUIRE(p->y == 2);

    Point* q = alloc.alloc<Point>();
    q->x = 3;
    q->y = 4;

    REQUIRE(p->x == 1);
    REQUIRE(q->x == 3);
}

TEST_CASE("linear_allocator: throws when full", "[allocator]") {
    LinearAllocator alloc(4096);
    REQUIRE_THROWS_AS(alloc.allocate(5000), std::bad_alloc);
}

TEST_CASE("linear_allocator: mark and reset_to", "[allocator]") {
    LinearAllocator alloc(4096);
    alloc.allocate(100);

    std::size_t marker = alloc.mark();
    alloc.allocate(100);
    alloc.allocate(100);
    REQUIRE(alloc.bytes_used() > marker);

    alloc.reset_to(marker);
    REQUIRE(alloc.bytes_used() == marker);
}

TEST_CASE("linear_allocator: minimum capacity is enforced", "[allocator]") {
    LinearAllocator alloc(100);
    REQUIRE(alloc.bytes_reserved() >= LinearAllocator::kMinCapacity);
}

TEST_CASE("linear_allocator: peak tracks high water mark", "[allocator]") {
    LinearAllocator alloc(4096);
    alloc.allocate(100);
    std::size_t peak1 = alloc.bytes_peak();
    alloc.allocate(500);
    std::size_t peak2 = alloc.bytes_peak();
    REQUIRE(peak2 > peak1);

    alloc.reset();
    REQUIRE(alloc.bytes_peak() == peak2);  // peak survives reset
}
