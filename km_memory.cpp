#include "km_memory.h"

#include <stdlib.h>

void MemCopy(void* dst, const void* src, uint64 numBytes)
{
    DEBUG_ASSERT(((const char*)dst + numBytes <= src)
                 || (dst >= (const char*)src + numBytes));
    // TODO maybe see about reimplementing this? would be informative
    memcpy(dst, src, numBytes);
}

void MemMove(void* dst, const void* src, uint64 numBytes)
{
    memmove(dst, src, numBytes);
}

void MemSet(void* dst, uint8 value, uint64 numBytes)
{
    uint8* d = (uint8*)dst;
    for (uint32 i = 0; i < numBytes; i++) {
        *d = value;
        d++;
    }
}

int MemComp(const void* mem1, const void* mem2, uint64 numBytes)
{
    return memcmp(mem1, mem2, numBytes);
}

void* StandardAllocator::Allocate(uint64 size)
{
    return malloc(size);
}

template <typename T> T* StandardAllocator::New()
{
    return (T*)Allocate(sizeof(T));
}

void* StandardAllocator::ReAllocate(void* memory, uint64 size)
{
    return realloc(memory, size);
}

void StandardAllocator::Free(void* memory)
{
    free(memory);
}

LinearAllocator::LinearAllocator(const LargeArray<uint8>& memory)
: used(0), capacity(memory.size), data(memory.data)
{
}

LinearAllocator::LinearAllocator(uint64 capacity, void* data)
: used(0), capacity(capacity), data(data)
{
}

void LinearAllocator::Initialize(const LargeArray<uint8>& memory)
{
    used = 0;
    capacity = memory.size;
    data = memory.data;
}

void LinearAllocator::Initialize(uint64 capacity, void* data)
{
    used = 0;
    this->capacity = capacity;
    this->data = data;
}

void* LinearAllocator::Allocate(uint64 size)
{
    if (used + size > capacity) {
        return nullptr;
    }

    uint64 start = used;
    used += size;
    return (void*)((uint8*)data + start);
}

template <typename T> T* LinearAllocator::New()
{
    return (T*)Allocate(sizeof(T));
}

template <typename T> T* LinearAllocator::New(uint64 n)
{
    return (T*)Allocate(n * sizeof(T));
}

template <typename T> Array<T> LinearAllocator::NewArray(uint32 size)
{
    return Array<T> { .size = size, .data = New<T>(size) };
}

void* LinearAllocator::ReAllocate(void* memory, uint64 size)
{
    void* newData = Allocate(size);
    if (newData == nullptr) {
        return nullptr;
    }

    // TODO extremely hacky way of copying memory, but otherwise, we would have had to know the previous alloc size
    uint64 diff = (uint64)newData - (uint64)memory;
    MemCopy(newData, memory, MinUInt64(diff, size));
    return newData;
}

void LinearAllocator::Free(void* memory)
{
    DEBUG_ASSERT(memory >= data);
    uint64 memoryInd = (uint64)memory - (uint64)data;
    DEBUG_ASSERT(memoryInd < capacity);
    used = memoryInd;
}

void LinearAllocator::Clear()
{
    used = 0;
}

uint64 LinearAllocator::GetRemainingBytes()
{
    return capacity - used;
}

LinearAllocatorState LinearAllocator::SaveState()
{
    LinearAllocatorState state;
    state.used = used;
    return state;
}

void LinearAllocator::LoadState(const LinearAllocatorState& state)
{
    used = state.used;
}