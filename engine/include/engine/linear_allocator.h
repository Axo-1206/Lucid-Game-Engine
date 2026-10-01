#pragma once

#include <cstddef>
#include <new>
#include <utility>

namespace engine {

// A single-block, bump-pointer allocator.
//
// allocate() advances an offset and returns a pointer into a fixed-size
// block. reset() rewinds the offset to 0, making the entire block reusable
// in O(1).
//
// The allocator does NOT grow. If an allocation would exceed the block's
// capacity, allocate() throws std::bad_alloc. Size the block for the
// expected workload.
//
// All pointers returned by allocate() remain valid until reset() is called.
// No individual free is possible.
//
// alloc<T>() constructs a T in place. It does NOT track or call the
// destructor. Use it only for trivially-destructible types, or ensure the
// type's destructor has no work to do.
//
// The allocator is not thread-safe.
class LinearAllocator {
public:
    // Minimum capacity: 4 KB. If a smaller value is passed, it is clamped.
    static constexpr std::size_t kMinCapacity = 4 * 1024;

    // Default capacity: 64 KB.
    static constexpr std::size_t kDefaultCapacity = 64 * 1024;

    explicit LinearAllocator(std::size_t capacity = kDefaultCapacity);
    ~LinearAllocator();

    LinearAllocator(const LinearAllocator&) = delete;
    LinearAllocator& operator=(const LinearAllocator&) = delete;
    LinearAllocator(LinearAllocator&&) noexcept;
    LinearAllocator& operator=(LinearAllocator&&) noexcept;

    // Allocate `size` bytes aligned to `alignment`.
    // `alignment` must be a power of two and at least 1.
    // Throws std::bad_alloc if the block doesn't have room.
    void* allocate(std::size_t size, std::size_t alignment = 8);

    // Allocate and construct a T. The constructor is called; the
    // destructor is not tracked.
    template <typename T, typename... Args>
    T* alloc(Args&&... args) {
        void* mem = allocate(sizeof(T), alignof(T));
        return new (mem) T(std::forward<Args>(args)...);
    }

    // Rewind the offset to 0. All pointers become invalid.
    void reset();

    // Mark the current offset. reset_to(marker) rewinds to it.
    std::size_t mark() const { return offset_; }
    void        reset_to(std::size_t marker);

    // Statistics.
    std::size_t bytes_used() const     { return offset_; }
    std::size_t bytes_reserved() const { return capacity_; }
    std::size_t bytes_peak() const     { return peak_; }

private:
    std::byte*  data_     = nullptr;
    std::size_t capacity_ = 0;
    std::size_t offset_   = 0;
    std::size_t peak_     = 0;
};

} // namespace engine
