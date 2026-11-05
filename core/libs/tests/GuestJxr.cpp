#include "prx/libc/include/Api.hpp"
#include "prx/libSceJxr/libSceJxr.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <stdexcept>
#include <utility>

namespace {

void Check(bool value, int line) { if (!value) { std::fprintf(stderr, "GuestJxr.cpp:%d: requirement failed\n", line); std::abort(); } }
#define Require(value) Check((value), __LINE__)

void* lastFreed = nullptr;
int frees = 0;
bool failAllocation = false;
std::size_t lastCount = 0;
std::size_t lastSize = 0;
void* lastAllocated = nullptr;
int callocCalls = 0;

void* APS5_VABI Allocate(std::size_t size) { return std::malloc(size); }
void APS5_VABI Free(void* pointer) {
    if (pointer != nullptr) {
        lastFreed = pointer;
        ++frees;
    }
    std::free(pointer);
}
void* APS5_VABI Reallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI Calloc(std::size_t count, std::size_t size) {
    ++callocCalls;
    lastCount = count;
    lastSize = size;
    lastAllocated = failAllocation ? nullptr : std::calloc(count, size);
    return lastAllocated;
}
void* APS5_VABI Align(std::size_t, std::size_t size) { return std::malloc(size); }
void* APS5_VABI Realign(void* pointer, std::size_t size, std::size_t) { return std::realloc(pointer, size); }
int APS5_VABI PosixAlign(void** pointer, std::size_t, std::size_t size) {
    *pointer = std::malloc(size);
    return *pointer ? 0 : 12;
}

void InitializeHeap() {
    alignas(16) static std::array<std::byte, 0x40> process{};
    alignas(16) static std::array<std::byte, 0x38> libc{};
    alignas(16) static std::array<std::byte, 0x78> replacement{};
    const auto write = [](auto& data, std::size_t offset, auto value) {
        std::memcpy(data.data() + offset, &value, sizeof(value));
    };
    write(process, 0, std::uint64_t{0x40});
    write(process, 8, std::uint32_t{0x4942524f});
    write(process, 0x38, libc.data());
    write(libc, 0, std::uint64_t{0x38});
    write(libc, 0x30, replacement.data());
    write(replacement, 0, std::uint64_t{0x78});
    write(replacement, 8, std::uint64_t{2});
    write(replacement, 0x20, reinterpret_cast<void*>(&Allocate));
    write(replacement, 0x28, reinterpret_cast<void*>(&Free));
    write(replacement, 0x30, reinterpret_cast<void*>(&Calloc));
    write(replacement, 0x38, reinterpret_cast<void*>(&Reallocate));
    write(replacement, 0x40, reinterpret_cast<void*>(&Align));
    write(replacement, 0x48, reinterpret_cast<void*>(&Realign));
    write(replacement, 0x50, reinterpret_cast<void*>(&PosixAlign));
    ApplicationHeapInitialize_nid_no_patch(process.data());
}


using MetadataSetter = decltype(&PKImageEncode_SetEXIFMetadata_WMP_nid_postfix);

void CheckMetadataSetter(MetadataSetter set, std::size_t offset) {
    alignas(8) std::array<std::byte, 0x140> encoder;
    encoder.fill(std::byte{0xa5});
    constexpr std::size_t headerDoneOffset = 0xb0;
    const auto write = [](auto& bytes, std::size_t at, auto value) {
        std::memcpy(bytes.data() + at, &value, sizeof(value));
    };
    const auto writeStorage = [&](auto& bytes, void* pointer, std::uint32_t count) {
        write(bytes, offset, pointer);
        write(bytes, offset + 8, count);
    };

    write(encoder, headerDoneOffset, std::int32_t{0});
    writeStorage(encoder, nullptr, 0);
    const auto empty = encoder;
    const std::array<std::uint8_t, 4> initial{0x33, 0, 0xb2, 0x81};
    std::array<std::uint8_t, 3> replacement{0xcc, 0x55, 0xaa};
    void* previous = nullptr;
    for (const auto& [data, count] : {std::pair{initial.data(), std::uint32_t{initial.size()}},
                                      std::pair{static_cast<const std::uint8_t*>(replacement.data()), std::uint32_t{replacement.size()}}}) {
        const auto beforeAllocations = callocCalls;
        const auto beforeFrees = frees;
        Require(set(encoder.data(), data, count) == 0);
        Require(callocCalls == beforeAllocations + 1 && lastCount == 1 && lastSize == count);
        Require(frees == beforeFrees + (previous ? 1 : 0));
        if (previous) Require(lastFreed == previous);
        Require(lastAllocated != nullptr && lastAllocated != data);
        Require(std::memcmp(lastAllocated, data, count) == 0);
        auto expected = empty;
        writeStorage(expected, lastAllocated, count);
        Require(encoder == expected);
        previous = lastAllocated;
    }
    replacement[0] = 0;
    Require(static_cast<const std::uint8_t*>(previous)[0] == 0xcc);

    const auto beforeAlias = encoder;
    const auto beforeAliasAllocations = callocCalls;
    const auto beforeAliasFrees = frees;
    bool aliasThrew = false;
    try { set(encoder.data(), static_cast<const std::uint8_t*>(previous), 3); }
    catch (const std::invalid_argument&) { aliasThrew = true; }
    Require(aliasThrew && encoder == beforeAlias);
    Require(callocCalls == beforeAliasAllocations && frees == beforeAliasFrees);
    aliasThrew = false;
    try { set(encoder.data(), static_cast<const std::uint8_t*>(previous) + 1, 2); }
    catch (const std::invalid_argument&) { aliasThrew = true; }
    Require(aliasThrew && encoder == beforeAlias);
    Require(callocCalls == beforeAliasAllocations && frees == beforeAliasFrees);

    const auto beforeFieldAllocations = callocCalls;
    const auto beforeFieldFrees = frees;
    const auto* countField = reinterpret_cast<const std::uint8_t*>(encoder.data() + offset + 8);
    Require(set(encoder.data(), countField, sizeof(std::uint32_t)) == 0);
    Require(callocCalls == beforeFieldAllocations + 1 && lastCount == 1 && lastSize == sizeof(std::uint32_t));
    Require(frees == beforeFieldFrees + 1 && lastFreed == previous);
    const std::array<std::uint8_t, sizeof(std::uint32_t)> clearedCount{};
    Require(std::memcmp(lastAllocated, clearedCount.data(), clearedCount.size()) == 0);
    auto expectedFieldCopy = empty;
    writeStorage(expectedFieldCopy, lastAllocated, sizeof(std::uint32_t));
    Require(encoder == expectedFieldCopy);
    previous = lastAllocated;

    write(encoder, headerDoneOffset, std::int32_t{1});
    const auto written = encoder;
    const auto beforeRejectAllocations = callocCalls;
    const auto beforeRejectFrees = frees;
    Require(set(encoder.data(), initial.data(), initial.size()) == -109);
    Require(encoder == written);
    Require(callocCalls == beforeRejectAllocations && frees == beforeRejectFrees);

    write(encoder, headerDoneOffset, std::int32_t{0});
    const auto beforeZeroAllocations = callocCalls;
    const auto beforeZeroFrees = frees;
    const auto zeroResult = set(encoder.data(), initial.data(), 0);
    Require(frees == beforeZeroFrees + 1 && lastFreed == previous);
    Require(callocCalls == beforeZeroAllocations + 1 && lastCount == 1 && lastSize == 0);
    Require(zeroResult == (lastAllocated ? 0 : -101));
    auto expected = empty;
    writeStorage(expected, lastAllocated, 0);
    Require(encoder == expected);

    Require(set(encoder.data(), initial.data(), initial.size()) == 0);
    previous = lastAllocated;
    const auto beforeFailureAllocations = callocCalls;
    const auto beforeFailureFrees = frees;
    failAllocation = true;
    Require(set(encoder.data(), replacement.data(), replacement.size()) == -101);
    failAllocation = false;
    Require(callocCalls == beforeFailureAllocations + 1 && lastCount == 1 && lastSize == replacement.size());
    Require(frees == beforeFailureFrees + 1 && lastFreed == previous);
    Require(encoder == empty);
}

}

int main() {
    InitializeHeap();
    void* memory = malloc_nid_postfix(64);
    Require(memory != nullptr);
    void* const allocated = memory;
    Require(PKFree_nid_postfix(&memory) == 0);
    Require(memory == nullptr);
    Require(frees == 1 && lastFreed == allocated);
    Require(PKFree_nid_postfix(&memory) == 0);
    Require(memory == nullptr);
    Require(frees == 1);
    Require(PKFree_nid_postfix(nullptr) == 0);
    Require(frees == 1);
    for (const auto size : {std::size_t{1}, std::size_t{257}, std::size_t{4097}, std::size_t{0}}) {
        memory = &memory;
        const auto calls = callocCalls;
        const auto result = PKAlloc_nid_postfix(&memory, size);
        Require(callocCalls == calls + 1 && lastCount == 1 && lastSize == size);
        Require(memory == lastAllocated);
        Require(result == (memory ? 0 : -101));
        if (size != 0) Require(memory != nullptr);
        auto* bytes = static_cast<unsigned char*>(memory);
        for (std::size_t i = 0; i < size; ++i) Require(bytes[i] == 0);
        if (size != 0) std::memset(memory, 0xa5, size);
        const auto previousFrees = frees;
        const auto pointer = memory;
        Require(PKFree_nid_postfix(&memory) == 0 && memory == nullptr);
        Require(frees == previousFrees + (pointer ? 1 : 0));
        if (pointer) Require(lastFreed == pointer);
    }
    failAllocation = true;
    memory = &memory;
    Require(PKAlloc_nid_postfix(&memory, 37) == -101);
    Require(memory == nullptr && lastCount == 1 && lastSize == 37);
    failAllocation = false;
    bool threw = false;
    try { PKAlloc_nid_postfix(nullptr, 1); }
    catch (const std::invalid_argument&) { threw = true; }
    Require(threw);
    CheckMetadataSetter(PKImageEncode_SetEXIFMetadata_WMP_nid_postfix, 0xd8);
    CheckMetadataSetter(PKImageEncode_SetGPSInfoMetadata_WMP_nid_postfix, 0xe8);
    CheckMetadataSetter(PKImageEncode_SetIPTCNAAMetadata_WMP_nid_postfix, 0xf8);
    CheckMetadataSetter(PKImageEncode_SetPhotoshopMetadata_WMP_nid_postfix, 0x118);
    return 0;
}
