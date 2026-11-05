#include "prx/libc/include/Api.hpp"
#include "prx/libSceJxr/libSceJxr.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <stdexcept>

namespace {

void Check(bool value, int line) { if (!value) { std::fprintf(stderr, "GuestJxr.cpp:%d: requirement failed\n", line); std::abort(); } }
#define Require(value) Check((value), __LINE__)

template <typename F>
void RequireThrows(F&& function) {
    try {
        function();
        std::abort();
    } catch (const std::invalid_argument&) {
    }
}

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

std::int64_t APS5_VABI MockGetPosition(Jxr::Stream*, std::size_t* pos) {
    *pos = 42;
    return 0;
}

int streamCloseCount = 0;
Jxr::Stream* closedStreamPointer = nullptr;

std::int64_t APS5_VABI MockClose(Jxr::Stream** stream) {
    ++streamCloseCount;
    if (stream) {
        closedStreamPointer = *stream;
        *stream = nullptr;
    }
    return 0;
}

std::int64_t APS5_VABI MockFailingClose(Jxr::Stream** stream) {
    ++streamCloseCount;
    if (stream) {
        closedStreamPointer = *stream;
        *stream = nullptr;
    }
    return -1;
}

bool GuidEquals(const JxrGuestGuid& lhs, const JxrGuestGuid& rhs) {
    return std::memcmp(&lhs, &rhs, sizeof(JxrGuestGuid)) == 0;
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

    Jxr::Stream mockStream{};
    mockStream.getPosition = &MockGetPosition;
    mockStream.close = &MockClose;

    Jxr::ImageDecode decoder{};
    std::memset(&decoder, 0x5A, sizeof(decoder));

    RequireThrows([&] { PKImageDecode_Initialize_nid_postfix(nullptr, &mockStream); });
    RequireThrows([&] { PKImageDecode_Initialize_nid_postfix(&decoder, nullptr); });
    Jxr::Stream invalidStream{};
    RequireThrows([&] { PKImageDecode_Initialize_nid_postfix(&decoder, &invalidStream); });

    Require(PKImageDecode_Initialize_nid_postfix(&decoder, &mockStream) == 0);
    Require(decoder.stream == &mockStream);
    Require(decoder.resolutionX == 96.0f);
    Require(decoder.resolutionY == 96.0f);
    Require(decoder.frameCount == 1);
    Require(decoder.startOffset == 42);
    Require(GuidEquals(decoder.pixelFormat, JxrGuest_GUID_PKPixelFormatDontCare_nid_no_patch));
    Require(std::all_of(decoder.misc.begin(), decoder.misc.end(), [](std::uint8_t b) { return b == 0; }));

    JxrGuestGuid queriedGuid{};
    RequireThrows([&] { PKImageDecode_GetPixelFormat_nid_postfix(nullptr, &queriedGuid); });
    RequireThrows([&] { PKImageDecode_GetPixelFormat_nid_postfix(&decoder, nullptr); });
    Require(PKImageDecode_GetPixelFormat_nid_postfix(&decoder, &queriedGuid) == 0);
    Require(GuidEquals(queriedGuid, JxrGuest_GUID_PKPixelFormatDontCare_nid_no_patch));

    JxrGuestGuid customGuid{0x12345678, 0x1234, 0x5678, {1, 2, 3, 4, 5, 6, 7, 8}};
    decoder.pixelFormat = customGuid;
    Require(PKImageDecode_GetPixelFormat_nid_postfix(&decoder, &queriedGuid) == 0);
    Require(GuidEquals(queriedGuid, customGuid));

    decoder.width = 1920;
    decoder.height = 1080;
    std::int32_t width = 0;
    std::int32_t height = 0;
    RequireThrows([&] { PKImageDecode_GetSize_nid_postfix(nullptr, &width, &height); });
    RequireThrows([&] { PKImageDecode_GetSize_nid_postfix(&decoder, nullptr, &height); });
    RequireThrows([&] { PKImageDecode_GetSize_nid_postfix(&decoder, &width, nullptr); });
    Require(PKImageDecode_GetSize_nid_postfix(&decoder, &width, &height) == 0);
    Require(width == 1920);
    Require(height == 1080);

    float resX = 0.0f;
    float resY = 0.0f;
    RequireThrows([&] { PKImageDecode_GetResolution_nid_postfix(nullptr, &resX, &resY); });
    RequireThrows([&] { PKImageDecode_GetResolution_nid_postfix(&decoder, nullptr, &resY); });
    RequireThrows([&] { PKImageDecode_GetResolution_nid_postfix(&decoder, &resX, nullptr); });
    Require(PKImageDecode_GetResolution_nid_postfix(&decoder, &resX, &resY) == 0);
    Require(resX == 96.0f);
    Require(resY == 96.0f);
    decoder.resolutionX = 300.0f;
    decoder.resolutionY = 300.0f;
    Require(PKImageDecode_GetResolution_nid_postfix(&decoder, &resX, &resY) == 0);
    Require(resX == 300.0f);
    Require(resY == 300.0f);

    std::uint32_t frameCount = 0;
    RequireThrows([&] { PKImageDecode_GetFrameCount_nid_postfix(nullptr, &frameCount); });
    RequireThrows([&] { PKImageDecode_GetFrameCount_nid_postfix(&decoder, nullptr); });
    Require(PKImageDecode_GetFrameCount_nid_postfix(&decoder, &frameCount) == 0);
    Require(frameCount == 1);
    decoder.frameCount = 4;
    Require(PKImageDecode_GetFrameCount_nid_postfix(&decoder, &frameCount) == 0);
    Require(frameCount == 4);

    decoder.width = 0;
    decoder.height = 0;
    Require(PKImageDecode_GetSize_nid_postfix(&decoder, &width, &height) == 0);
    Require(width == 0 && height == 0);
    decoder.width = 0x7FFFFFFF;
    decoder.height = 0x7FFFFFFF;
    Require(PKImageDecode_GetSize_nid_postfix(&decoder, &width, &height) == 0);
    Require(width == 0x7FFFFFFF && height == 0x7FFFFFFF);

    decoder.resolutionX = 0.0f;
    decoder.resolutionY = -1.0f;
    Require(PKImageDecode_GetResolution_nid_postfix(&decoder, &resX, &resY) == 0);
    Require(resX == 0.0f && resY == -1.0f);

    decoder.frameCount = 0;
    Require(PKImageDecode_GetFrameCount_nid_postfix(&decoder, &frameCount) == 0);
    Require(frameCount == 0);
    decoder.frameCount = 0xFFFFFFFF;
    Require(PKImageDecode_GetFrameCount_nid_postfix(&decoder, &frameCount) == 0);
    Require(frameCount == 0xFFFFFFFF);


    decoder.initialize = &PKImageDecode_Initialize_nid_postfix;
    decoder.getPixelFormat = &PKImageDecode_GetPixelFormat_nid_postfix;
    decoder.getSize = &PKImageDecode_GetSize_nid_postfix;
    decoder.getResolution = &PKImageDecode_GetResolution_nid_postfix;
    decoder.getColorContext = &PKImageDecode_GetColorContext_nid_postfix;
    decoder.getDescriptiveMetadata = &PKImageDecode_GetDescriptiveMetadata_nid_postfix;
    decoder.getFrameCount = &PKImageDecode_GetFrameCount_nid_postfix;
    decoder.release = &PKImageDecode_Release_nid_postfix;

    Require(decoder.initialize(&decoder, &mockStream) == 0);
    Require(decoder.getPixelFormat(&decoder, &queriedGuid) == 0);
    Require(decoder.getSize(&decoder, &width, &height) == 0);
    Require(decoder.getResolution(&decoder, &resX, &resY) == 0);
    Require(decoder.getFrameCount(&decoder, &frameCount) == 0);
    Require(decoder.getColorContext(&decoder, nullptr, nullptr) == -2);
    Require(decoder.getDescriptiveMetadata(&decoder, nullptr) == -2);

    std::uint8_t colorBuffer[16]{};
    std::uint32_t colorSize = sizeof(colorBuffer);
    RequireThrows([&] { PKImageDecode_GetColorContext_nid_postfix(nullptr, colorBuffer, &colorSize); });
    Require(PKImageDecode_GetColorContext_nid_postfix(&decoder, nullptr, nullptr) == -2);
    Require(PKImageDecode_GetColorContext_nid_postfix(&decoder, colorBuffer, &colorSize) == -2);

    std::uint8_t descMetadata[32]{};
    RequireThrows([&] { PKImageDecode_GetDescriptiveMetadata_nid_postfix(nullptr, descMetadata); });
    Require(PKImageDecode_GetDescriptiveMetadata_nid_postfix(&decoder, nullptr) == -2);
    Require(PKImageDecode_GetDescriptiveMetadata_nid_postfix(&decoder, descMetadata) == -2);

    Require(PKImageDecode_Release_nid_postfix(nullptr) == 0);
    Jxr::ImageDecode* nullDecoder = nullptr;
    Require(PKImageDecode_Release_nid_postfix(&nullDecoder) == 0);
    Require(nullDecoder == nullptr);

    auto* heapDecoder1 = static_cast<Jxr::ImageDecode*>(malloc_nid_postfix(sizeof(Jxr::ImageDecode)));
    Require(heapDecoder1 != nullptr);
    std::memset(heapDecoder1, 0, sizeof(Jxr::ImageDecode));
    heapDecoder1->stream = &mockStream;
    heapDecoder1->streamOwner = 0;
    const int freesBeforeRelease1 = frees;
    streamCloseCount = 0;
    Require(PKImageDecode_Release_nid_postfix(&heapDecoder1) == 0);
    Require(heapDecoder1 == nullptr);
    Require(frees == freesBeforeRelease1 + 1);
    Require(streamCloseCount == 0);

    auto* heapDecoder2 = static_cast<Jxr::ImageDecode*>(malloc_nid_postfix(sizeof(Jxr::ImageDecode)));
    Require(heapDecoder2 != nullptr);
    std::memset(heapDecoder2, 0, sizeof(Jxr::ImageDecode));
    Jxr::Stream ownedStream{};
    ownedStream.close = &MockClose;
    heapDecoder2->stream = &ownedStream;
    heapDecoder2->streamOwner = 1;
    const int freesBeforeRelease2 = frees;
    streamCloseCount = 0;
    closedStreamPointer = nullptr;
    Require(PKImageDecode_Release_nid_postfix(&heapDecoder2) == 0);
    Require(heapDecoder2 == nullptr);
    Require(frees == freesBeforeRelease2 + 1);
    Require(streamCloseCount == 1);
    Require(closedStreamPointer == &ownedStream);
    Require(heapDecoder2 == nullptr);

    auto* heapDecoder3 = static_cast<Jxr::ImageDecode*>(malloc_nid_postfix(sizeof(Jxr::ImageDecode)));
    Require(heapDecoder3 != nullptr);
    std::memset(heapDecoder3, 0, sizeof(Jxr::ImageDecode));
    heapDecoder3->stream = nullptr;
    heapDecoder3->streamOwner = 1;
    const int freesBeforeRelease3 = frees;
    streamCloseCount = 0;
    Require(PKImageDecode_Release_nid_postfix(&heapDecoder3) == 0);
    Require(heapDecoder3 == nullptr);
    Require(frees == freesBeforeRelease3 + 1);
    Require(streamCloseCount == 0);

    auto* heapDecoder4 = static_cast<Jxr::ImageDecode*>(malloc_nid_postfix(sizeof(Jxr::ImageDecode)));
    Require(heapDecoder4 != nullptr);
    std::memset(heapDecoder4, 0, sizeof(Jxr::ImageDecode));
    Jxr::Stream unclosableStream{};
    heapDecoder4->stream = &unclosableStream;
    heapDecoder4->streamOwner = 1;
    const int freesBeforeRelease4 = frees;
    streamCloseCount = 0;
    Require(PKImageDecode_Release_nid_postfix(&heapDecoder4) == 0);
    Require(heapDecoder4 == nullptr);
    Require(frees == freesBeforeRelease4 + 1);
    Require(streamCloseCount == 0);

    auto* heapDecoder5 = static_cast<Jxr::ImageDecode*>(malloc_nid_postfix(sizeof(Jxr::ImageDecode)));
    Require(heapDecoder5 != nullptr);
    std::memset(heapDecoder5, 0, sizeof(Jxr::ImageDecode));
    Jxr::Stream failingStream{};
    failingStream.close = &MockFailingClose;
    heapDecoder5->stream = &failingStream;
    heapDecoder5->streamOwner = 1;
    const int freesBeforeRelease5 = frees;
    streamCloseCount = 0;
    closedStreamPointer = nullptr;
    Require(PKImageDecode_Release_nid_postfix(&heapDecoder5) == 0);
    Require(heapDecoder5 == nullptr);
    Require(frees == freesBeforeRelease5 + 1);
    Require(streamCloseCount == 1);
    Require(closedStreamPointer == &failingStream);

    auto* heapDecoder6 = static_cast<Jxr::ImageDecode*>(malloc_nid_postfix(sizeof(Jxr::ImageDecode)));
    Require(heapDecoder6 != nullptr);
    std::memset(heapDecoder6, 0, sizeof(Jxr::ImageDecode));
    heapDecoder6->release = &PKImageDecode_Release_nid_postfix;
    const int freesBeforeRelease6 = frees;
    Require(heapDecoder6->release(&heapDecoder6) == 0);
    Require(heapDecoder6 == nullptr);
    Require(frees == freesBeforeRelease6 + 1);

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
    return 0;
}
