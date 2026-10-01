#include "engine/linear_allocator.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace engine {

namespace {

bool is_power_of_two(std::size_t value) {
    return value != 0 && (value & (value - 1)) == 0;
}

std::size_t align_up(std::size_t value, std::size_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

} // namespace

LinearAllocator::LinearAllocator(std::size_t capacity) {
    if (capacity < kMinCapacity) capacity = kMinCapacity;
    capacity_ = capacity;

    // Ensure the backing store is aligned to a 64-byte boundary so callers can
    // request 64-byte aligned allocations without depending on the platform's
    // default heap alignment.
    constexpr std::size_t kBackingAlignment = 64;
    data_ = static_cast<std::byte*>(::operator new[](capacity_, std::align_val_t(kBackingAlignment)));
}

LinearAllocator::~LinearAllocator() {
    ::operator delete[](data_, std::align_val_t(64));
    data_ = nullptr;
    capacity_ = 0;
    offset_ = 0;
    peak_ = 0;
}

LinearAllocator::LinearAllocator(LinearAllocator&& other) noexcept
    : data_(other.data_)
    , capacity_(other.capacity_)
    , offset_(other.offset_)
    , peak_(other.peak_)
{
    other.data_ = nullptr;
    other.capacity_ = 0;
    other.offset_ = 0;
    other.peak_ = 0;
}

LinearAllocator& LinearAllocator::operator=(LinearAllocator&& other) noexcept {
    if (this != &other) {
        ::operator delete[](data_, std::align_val_t(64));
        data_ = other.data_;
        capacity_ = other.capacity_;
        offset_ = other.offset_;
        peak_ = other.peak_;
        other.data_ = nullptr;
        other.capacity_ = 0;
        other.offset_ = 0;
        other.peak_ = 0;
    }
    return *this;
}

void* LinearAllocator::allocate(std::size_t size, std::size_t alignment) {
    if (!is_power_of_two(alignment)) {
        throw std::invalid_argument("LinearAllocator: alignment must be a power of two");
    }

    const std::size_t aligned_offset = align_up(offset_, alignment);

    // Overflow check.
    if (aligned_offset > capacity_ || size > capacity_ - aligned_offset) {
        throw std::bad_alloc();
    }

    void* result = data_ + aligned_offset;
    offset_ = aligned_offset + size;

    if (offset_ > peak_) peak_ = offset_;

    return result;
}

void LinearAllocator::reset() {
    offset_ = 0;
}

void LinearAllocator::reset_to(std::size_t marker) {
    if (marker > offset_) {
        throw std::invalid_argument("LinearAllocator: reset_to marker is past the current offset");
    }
    offset_ = marker;
}

} // namespace engine
