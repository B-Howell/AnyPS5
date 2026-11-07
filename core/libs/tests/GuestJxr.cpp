#include "prx/libc/include/Api.hpp"
#include "prx/libSceJxr/libSceJxr.hpp"
#include "prx/libSceJxr/src/EncoderMetadata.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <vector>

extern "C" {
void JxrReferenceSlideOneMBRow_nid_no_patch(void* rows, std::size_t channels, std::size_t mbWidth, int top,
                                            int bottom);
void JxrReferenceUpdatePostProcInfo_nid_no_patch(void* rows, std::int32_t* mb, std::size_t mbX, std::size_t channel);
void JxrReferencePostProcMB_nid_no_patch(void* rows, std::int32_t* p0, std::int32_t* p1, std::size_t mbX,
                                         std::size_t channel, int threshold);
void JxrReferencePostProcBlock_nid_no_patch(void* rows, std::int32_t* p0, std::int32_t* p1, std::size_t mbX,
                                            std::size_t channel, int threshold);
}


namespace {

void Check(bool value, int line) { if (!value) { std::fprintf(stderr, "GuestJxr.cpp:%d: requirement failed\n", line); std::abort(); } }
#define Require(value) Check((value), __LINE__)

template<typename TAction>
void RequireInvalidArgument(TAction action) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    std::abort();
}


void* lastFreed = nullptr;
int frees = 0;
std::array<void*, 32> freeLog{};
bool failAllocation = false;
bool fillMalloc = false;
int mallocCalls = 0;
int failMallocAt = -1;
std::array<void*, 32> mallocLog{};
std::array<std::size_t, 32> mallocSizeLog{};
std::size_t lastCount = 0;
std::size_t lastSize = 0;
void* lastAllocated = nullptr;
int callocCalls = 0;

void* APS5_VABI Allocate(std::size_t size) {
    ++mallocCalls;
    void* pointer = mallocCalls == failMallocAt ? nullptr : std::malloc(size);
    if (pointer && fillMalloc) std::memset(pointer, 0xa5, size);
    if (mallocCalls <= static_cast<int>(mallocLog.size())) {
        mallocLog[static_cast<std::size_t>(mallocCalls - 1)] = pointer;
        mallocSizeLog[static_cast<std::size_t>(mallocCalls - 1)] = size;
    }
    return pointer;
}
void APS5_VABI Free(void* pointer) {
    if (pointer != nullptr) {
        lastFreed = pointer;
        if (frees < static_cast<int>(freeLog.size())) freeLog[static_cast<std::size_t>(frees)] = pointer;
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

void TestExifMetadata() {
    struct Fixture {
        Jxr::ImageEncodeMetadataPrefix encoder;
        std::array<std::byte, 32> trailing;
    } fixture;
    std::memset(&fixture, 0xa5, sizeof(fixture));
    fixture.encoder.headerDone = 0;
    fixture.encoder.exifMetadata = nullptr;
    fixture.encoder.exifMetadataByteCount = 0xfeed;
    auto& encoder = fixture.encoder;
    auto* const encoderPointer = reinterpret_cast<tagPKImageEncode*>(&encoder);
    const auto snapshot = [&] {
        std::array<std::byte, sizeof(fixture)> bytes;
        std::memcpy(bytes.data(), &fixture, sizeof(fixture));
        return bytes;
    };
    const auto requireOnlyExif = [&](const std::array<std::byte, sizeof(fixture)>& before, const std::uint8_t* pointer,
                                     std::uint32_t size) {
        auto expected = before;
        std::memcpy(expected.data() + offsetof(Fixture, encoder) + offsetof(Jxr::ImageEncodeMetadataPrefix, exifMetadata),
                    &pointer, sizeof(pointer));
        std::memcpy(expected.data() + offsetof(Fixture, encoder) + offsetof(Jxr::ImageEncodeMetadataPrefix, exifMetadataByteCount),
                    &size, sizeof(size));
        Require(std::memcmp(expected.data(), &fixture, sizeof(fixture)) == 0);
    };
    const std::array<std::uint8_t, 5> first {0x11, 0x22, 0x33, 0x44, 0x55};
    const std::array<std::uint8_t, 3> second {0xa1, 0xb2, 0xc3};
    auto before = snapshot();
    const auto firstCalloc = callocCalls;
    Require(PKImageEncode_SetEXIFMetadata_WMP_nid_postfix(encoderPointer, first.data(), static_cast<std::uint32_t>(first.size())) == 0);
    Require(callocCalls == firstCalloc + 1 && lastCount == 1 && lastSize == first.size());
    Require(encoder.exifMetadata != first.data() && encoder.exifMetadataByteCount == first.size());
    Require(std::memcmp(encoder.exifMetadata, first.data(), static_cast<std::uint32_t>(first.size())) == 0);
    requireOnlyExif(before, encoder.exifMetadata, static_cast<std::uint32_t>(first.size()));

    auto* const previous = encoder.exifMetadata;
    const auto previousFrees = frees;
    before = snapshot();
    Require(PKImageEncode_SetEXIFMetadata_WMP_nid_postfix(encoderPointer, second.data(), static_cast<std::uint32_t>(second.size())) == 0);
    Require(frees == previousFrees + 1 && lastFreed == previous);
    Require(encoder.exifMetadata != second.data() && encoder.exifMetadataByteCount == second.size());
    Require(std::memcmp(encoder.exifMetadata, second.data(), static_cast<std::uint32_t>(second.size())) == 0);
    requireOnlyExif(before, encoder.exifMetadata, static_cast<std::uint32_t>(second.size()));

    auto* const guardedPointer = encoder.exifMetadata;
    const auto guardedSize = encoder.exifMetadataByteCount;
    const auto guardedFrees = frees;
    const auto guardedCallocs = callocCalls;
    encoder.headerDone = 1;
    before = snapshot();
    Require(PKImageEncode_SetEXIFMetadata_WMP_nid_postfix(encoderPointer, first.data(), static_cast<std::uint32_t>(first.size())) == -109);
    Require(PKImageEncode_SetEXIFMetadata_WMP_nid_postfix(encoderPointer, nullptr, static_cast<std::uint32_t>(first.size())) == -109);
    Require(encoder.exifMetadata == guardedPointer && encoder.exifMetadataByteCount == guardedSize);
    Require(snapshot() == before);
    Require(frees == guardedFrees && callocCalls == guardedCallocs);
    encoder.headerDone = 0;

    bool threw = false;
    try { PKImageEncode_SetEXIFMetadata_WMP_nid_postfix(nullptr, first.data(), static_cast<std::uint32_t>(first.size())); }
    catch (const std::invalid_argument&) { threw = true; }
    Require(threw);
    threw = false;
    before = snapshot();
    const auto nullInputFrees = frees;
    const auto nullInputCallocs = callocCalls;
    try { PKImageEncode_SetEXIFMetadata_WMP_nid_postfix(encoderPointer, nullptr, static_cast<std::uint32_t>(first.size())); }
    catch (const std::invalid_argument&) { threw = true; }
    Require(threw && frees == nullInputFrees + 1 && lastFreed == guardedPointer);
    Require(callocCalls == nullInputCallocs + 1 && encoder.exifMetadata == lastAllocated && encoder.exifMetadataByteCount == 0);
    requireOnlyExif(before, encoder.exifMetadata, 0);
    void* nullInputAllocation = encoder.exifMetadata;
    Require(PKFree_nid_postfix(&nullInputAllocation) == 0);
    encoder.exifMetadata = static_cast<std::uint8_t*>(nullInputAllocation);

    Require(PKImageEncode_SetEXIFMetadata_WMP_nid_postfix(encoderPointer, second.data(), static_cast<std::uint32_t>(second.size())) == 0);
    auto* const failurePrevious = encoder.exifMetadata;
    const auto failedCallocs = callocCalls;
    const auto failedFrees = frees;
    before = snapshot();
    failAllocation = true;
    Require(PKImageEncode_SetEXIFMetadata_WMP_nid_postfix(encoderPointer, first.data(), static_cast<std::uint32_t>(first.size())) == -101);
    failAllocation = false;
    Require(callocCalls == failedCallocs + 1 && lastCount == 1 && lastSize == first.size());
    Require(lastAllocated == nullptr && encoder.exifMetadata == nullptr && encoder.exifMetadataByteCount == 0);
    Require(frees == failedFrees + 1 && lastFreed == failurePrevious);
    requireOnlyExif(before, nullptr, 0);

    failAllocation = false;
    Require(PKImageEncode_SetEXIFMetadata_WMP_nid_postfix(encoderPointer, second.data(), static_cast<std::uint32_t>(second.size())) == 0);
    auto* const nullFailurePrevious = encoder.exifMetadata;
    before = snapshot();
    const auto failedNullCallocs = callocCalls;
    const auto failedNullFrees = frees;
    failAllocation = true;
    Require(PKImageEncode_SetEXIFMetadata_WMP_nid_postfix(encoderPointer, nullptr, static_cast<std::uint32_t>(first.size())) == -101);
    Require(callocCalls == failedNullCallocs + 1 && frees == failedNullFrees + 1 && lastFreed == nullFailurePrevious);
    Require(encoder.exifMetadata == nullptr && encoder.exifMetadataByteCount == 0);
    requireOnlyExif(before, nullptr, 0);
    failAllocation = false;

    std::uint8_t emptyInput = 0;
    before = snapshot();
    const auto zeroResult = PKImageEncode_SetEXIFMetadata_WMP_nid_postfix(encoderPointer, &emptyInput, 0);
    Require(zeroResult == (lastAllocated ? 0 : -101));
    Require(encoder.exifMetadata == lastAllocated && encoder.exifMetadataByteCount == 0);
    requireOnlyExif(before, encoder.exifMetadata, 0);
    if (encoder.exifMetadata) {
        void* emptyAllocation = encoder.exifMetadata;
        Require(PKFree_nid_postfix(&emptyAllocation) == 0);
        encoder.exifMetadata = static_cast<std::uint8_t*>(emptyAllocation);
    }
}

void TestInitPostProc() {
    fillMalloc = true;
    JxrGuestPostProcInfo sentinel{};
    JxrGuestPostProcInfo* slots[4][2];
    for (auto& channel : slots)
        for (auto& slot : channel) slot = &sentinel;

    constexpr std::size_t width = 2;
    const auto firstAllocation = mallocCalls;
    Require(initPostProc_nid_postfix(slots, width, 2) == 0);
    Require(mallocCalls == firstAllocation + 4);
    for (std::size_t channel = 0; channel < 2; ++channel) {
        for (std::size_t slot = 0; slot < 2; ++slot) {
            const auto logIndex = static_cast<std::size_t>(firstAllocation + channel * 2 + slot);
            auto* const base = static_cast<JxrGuestPostProcInfo*>(mallocLog[logIndex]);
            Require(base != nullptr && slots[channel][slot] == base + 1);
            Require(mallocSizeLog[logIndex] == (width + 2) * sizeof(JxrGuestPostProcInfo));
            const auto checkBoundary = [](const JxrGuestPostProcInfo* value) {
                Require(value->mbTexture == 3);
                for (const auto& row : value->blockTexture)
                    for (const auto texture : row) Require(texture == 3);
            };
            checkBoundary(slots[channel][slot] - 1);
            checkBoundary(slots[channel][slot] + width);
            for (std::size_t i = 0; i < width; ++i) {
                const auto* bytes = reinterpret_cast<const unsigned char*>(slots[channel][slot] + i);
                for (std::size_t j = 0; j < sizeof(JxrGuestPostProcInfo); ++j) Require(bytes[j] == 0xa5);
            }
            Require(std::memcmp(base, slots[channel][slot] + width, sizeof(*base)) == 0);
        }
    }
    for (std::size_t channel = 2; channel < 4; ++channel)
        for (const auto* slot : slots[channel]) Require(slot == &sentinel);
    for (std::size_t channel = 0; channel < 2; ++channel)
        for (std::size_t slot = 0; slot < 2; ++slot) free_nid_postfix(slots[channel][slot] - 1);

    const auto allocationsBeforeZeroChannels = mallocCalls;
    Require(initPostProc_nid_postfix(nullptr, std::numeric_limits<std::size_t>::max(), 0) == 0);
    Require(mallocCalls == allocationsBeforeZeroChannels);

    JxrGuestPostProcInfo* overflowSlots[1][2] = {{&sentinel, &sentinel}};
    const auto allocationsBeforeOverflow = mallocCalls;
    constexpr auto overflowWidth = std::size_t{745} * 65536 - 2;
    Require(initPostProc_nid_postfix(overflowSlots, overflowWidth, 1) == -1);
    Require(mallocCalls == allocationsBeforeOverflow);
    Require(overflowSlots[0][0] == &sentinel && overflowSlots[0][1] == &sentinel);

    JxrGuestPostProcInfo* partialSlots[3][2];
    for (auto& channel : partialSlots)
        for (auto& slot : channel) slot = &sentinel;
    const auto firstPartialAllocation = mallocCalls;
    failMallocAt = mallocCalls + 3;
    Require(initPostProc_nid_postfix(partialSlots, 1, 2) == -1);
    failMallocAt = -1;
    Require(mallocCalls == firstPartialAllocation + 3);
    Require(partialSlots[0][0] == static_cast<JxrGuestPostProcInfo*>(mallocLog[static_cast<std::size_t>(firstPartialAllocation)]) + 1);
    Require(partialSlots[0][1] == static_cast<JxrGuestPostProcInfo*>(mallocLog[static_cast<std::size_t>(firstPartialAllocation + 1)]) + 1);
    Require(partialSlots[1][0] == nullptr && partialSlots[1][1] == &sentinel);
    Require(partialSlots[2][0] == &sentinel && partialSlots[2][1] == &sentinel);
    free_nid_postfix(partialSlots[0][0] - 1);
    free_nid_postfix(partialSlots[0][1] - 1);
    fillMalloc = false;
}

void TestTermPostProc() {
    JxrGuestPostProcInfo sentinel{};
    JxrGuestPostProcInfo* slots[3][2];
    for (auto& channel : slots)
        for (auto& slot : channel) slot = &sentinel;

    std::array<JxrGuestPostProcInfo*, 3> base{};
    const auto firstAllocation = mallocCalls;
    for (auto& pointer : base) {
        pointer = static_cast<JxrGuestPostProcInfo*>(malloc_nid_postfix(sizeof(JxrGuestPostProcInfo)));
        Require(pointer != nullptr);
    }
    Require(mallocCalls == firstAllocation + 3);
    for (std::size_t i = 0; i < base.size(); ++i) {
        const auto index = static_cast<std::size_t>(firstAllocation) + i;
        Require(mallocLog[index] == base[i]);
        Require(mallocSizeLog[index] == 88);
    }

    slots[0][0] = base[0] + 1;
    slots[0][1] = nullptr;
    slots[1][0] = base[1] + 1;
    slots[1][1] = base[2] + 1;
    const auto firstFree = frees;
    termPostProc_nid_postfix(slots, 2);
    Require(frees == firstFree + 3);
    for (std::size_t i = 0; i < base.size(); ++i)
        Require(freeLog[static_cast<std::size_t>(firstFree) + i] == base[i]);
    Require(slots[0][0] == base[0] + 1 && slots[0][1] == nullptr);
    Require(slots[1][0] == base[1] + 1 && slots[1][1] == base[2] + 1);
    for (std::size_t slot = 0; slot < 2; ++slot) Require(slots[2][slot] == &sentinel);

    const auto freesBeforeZeroChannels = frees;
    termPostProc_nid_postfix(nullptr, 0);
    Require(frees == freesBeforeZeroChannels);

    bool threw = false;
    try { termPostProc_nid_postfix(nullptr, 1); }
    catch (const std::invalid_argument&) { threw = true; }
    Require(threw);
}

bool Same(const JxrGuestPostProcInfo& left, const JxrGuestPostProcInfo& right) {
    return left.mbDc == right.mbDc && left.mbTexture == right.mbTexture &&
           std::memcmp(left.blockDc, right.blockDc, sizeof(left.blockDc)) == 0 &&
           std::memcmp(left.blockTexture, right.blockTexture, sizeof(left.blockTexture)) == 0;
}

std::uint32_t seed = 0x2545f491u;

std::int32_t Next(std::int32_t low, std::int32_t high) {
    seed = seed * 1664525u + 1013904223u;
    return low + static_cast<std::int32_t>((seed >> 8) % static_cast<std::uint32_t>(high - low + 1));
}

struct Rows {
    std::vector<JxrGuestPostProcInfo> storage[2][2];
    JxrGuestPostProcInfo* rows[16][2]{};

    explicit Rows(std::size_t width) {
        for (std::size_t channel = 0; channel < 2; ++channel) {
            for (std::size_t row = 0; row < 2; ++row) {
                storage[channel][row].assign(width + 2, JxrGuestPostProcInfo{});
                rows[channel][row] = storage[channel][row].data() + 1;
            }
        }
    }

    Rows(const Rows& other) {
        for (std::size_t channel = 0; channel < 2; ++channel) {
            for (std::size_t row = 0; row < 2; ++row) {
                storage[channel][row] = other.storage[channel][row];
                rows[channel][row] = storage[channel][row].data() + 1;
            }
        }
    }

    Rows& operator=(const Rows&) = delete;
};

void Randomize(Rows& rows, std::size_t width) {
    static constexpr std::uint8_t textures[] = {0, 0, 0, 1, 2, 3};
    for (auto& channel : rows.storage) {
        for (auto& row : channel) {
            for (std::size_t index = 0; index < width + 2; ++index) {
                auto& info = row[index];
                info.mbDc = Next(0, 4);
                info.mbTexture = static_cast<std::uint8_t>(Next(0, 1) * 3);
                for (std::size_t j = 0; j < 4; ++j) {
                    for (std::size_t i = 0; i < 4; ++i) {
                        info.blockDc[j][i] = Next(0, 4);
                        info.blockTexture[j][i] = textures[Next(0, 5)];
                    }
                }
            }
        }
    }
}

bool SameRows(const Rows& left, const Rows& right, std::size_t width) {
    for (std::size_t channel = 0; channel < 2; ++channel)
        for (std::size_t row = 0; row < 2; ++row)
            for (std::size_t index = 0; index < width + 2; ++index)
                if (!Same(left.storage[channel][row][index], right.storage[channel][row][index])) return false;
    return true;
}

void TestSlide() {
    constexpr std::size_t width = 3;
    for (int top = 0; top < 2; ++top) {
        for (int bottom = 0; bottom < 2; ++bottom) {
            Rows guest(width);
            Randomize(guest, width);
            for (std::size_t channel = 0; channel < 2; ++channel) {
                for (std::size_t row = 0; row < 2; ++row) {
                    guest.rows[channel][row][-1].mbTexture = 3;
                    std::memset(guest.rows[channel][row][-1].blockTexture, 3, 16);
                }
            }
            Rows reference(guest);
            const Rows before(guest);
            JxrGuestPostProcInfo* previous[2][2]{};
            for (std::size_t channel = 0; channel < 2; ++channel)
                for (std::size_t row = 0; row < 2; ++row) previous[channel][row] = guest.rows[channel][row];
            slideOneMBRow_nid_postfix(guest.rows, 2, width, top, bottom);
            JxrReferenceSlideOneMBRow_nid_no_patch(reference.rows, 2, width, top, bottom);
            Require(SameRows(guest, reference, width));
            for (std::size_t channel = 0; channel < 2; ++channel) {
                Require(guest.rows[channel][0] == previous[channel][1] && guest.rows[channel][1] == previous[channel][0]);
                Require(reference.rows[channel][0] == reference.storage[channel][1].data() + 1);
                for (std::size_t row = 0; row < 2; ++row) {
                    const std::size_t source = row == 0 ? 1 : 0;
                    const bool cleared = row == 0 ? top != 0 : bottom != 0;
                    const auto* info = guest.rows[channel][row];
                    const auto& original = before.storage[channel][source];
                    for (std::size_t index = 0; index < width; ++index)
                        Require(Same(info[index], cleared ? original[0] : original[index + 1]));
                    Require(Same(info[-1], original[0]) && Same(info[width], original[width + 1]));
                }
            }
        }
    }
    Rows unchanged(width);
    slideOneMBRow_nid_postfix(unchanged.rows, 0, width, 1, 1);
    Require(unchanged.rows[0][0] == unchanged.storage[0][0].data() + 1);
    slideOneMBRow_nid_postfix(nullptr, 0, width, 1, 1);
    RequireInvalidArgument([] { slideOneMBRow_nid_postfix(nullptr, 1, 3, 0, 0); });
    unchanged.rows[1][1] = nullptr;
    RequireInvalidArgument([&] { slideOneMBRow_nid_postfix(unchanged.rows, 2, width, 1, 0); });
}

void TestUpdate() {
    Rows rows(3);
    std::vector<std::int32_t> mb(256, 0);
    mb[0] = 7;
    mb[16] = 1;
    updatePostProcInfo_nid_postfix(rows.rows, mb.data(), 2, 1);
    const auto& info = rows.rows[1][1][2];
    Require(info.mbDc == 7 && info.mbTexture == 3);
    for (const auto& row : info.blockTexture)
        for (auto texture : row) Require(texture == 0);
    mb[16] = 0;
    mb[64 + 32 + 5] = -2;
    updatePostProcInfo_nid_postfix(rows.rows, mb.data(), 2, 1);
    Require(info.mbTexture == 0 && info.blockTexture[2][1] == 3 && info.blockTexture[0][0] == 0);

    for (int round = 0; round < 64; ++round) {
        Rows guest(3);
        Randomize(guest, 3);
        Rows reference(guest);
        for (auto& value : mb) value = Next(0, 7) == 0 ? Next(-3, 3) : 0;
        const auto mbX = static_cast<std::size_t>(round % 3);
        const auto channel = static_cast<std::size_t>(round % 2);
        updatePostProcInfo_nid_postfix(guest.rows, mb.data(), mbX, channel);
        JxrReferenceUpdatePostProcInfo_nid_no_patch(reference.rows, mb.data(), mbX, channel);
        Require(SameRows(guest, reference, 3));
    }
    RequireInvalidArgument([&] { updatePostProcInfo_nid_postfix(rows.rows, nullptr, 0, 0); });
    RequireInvalidArgument([&] { updatePostProcInfo_nid_postfix(rows.rows, mb.data(), 0, 16); });
    RequireInvalidArgument([&] { updatePostProcInfo_nid_postfix(nullptr, mb.data(), 0, 0); });
}

using PostProcess = void APS5_VABI (*)(JxrPostProcRows, std::int32_t*, std::int32_t*, std::size_t, std::size_t, int);
using ReferencePostProcess = void (*)(void*, std::int32_t*, std::int32_t*, std::size_t, std::size_t, int);

void ComparePostProcess(PostProcess guestFunction, ReferencePostProcess referenceFunction) {
    static constexpr int thresholds[] = {0, 1, 2, 8, 64};
    for (int round = 0; round < 200; ++round) {
        Rows guest(3);
        Randomize(guest, 3);
        Rows reference(guest);
        std::vector<std::int32_t> pixels[2];
        for (auto& row : pixels) {
            row.resize(3 * 256);
            for (auto& value : row) value = Next(-64, 64);
        }
        std::vector<std::int32_t> expected[2] = {pixels[0], pixels[1]};
        const auto mbX = static_cast<std::size_t>(1 + round % 2);
        const auto channel = static_cast<std::size_t>((round / 2) % 2);
        const int threshold = thresholds[round % 5];
        guestFunction(guest.rows, pixels[0].data() + 256 * mbX, pixels[1].data() + 256 * mbX, mbX, channel, threshold);
        referenceFunction(reference.rows, expected[0].data() + 256 * mbX, expected[1].data() + 256 * mbX, mbX, channel,
                          threshold);
        Require(pixels[0] == expected[0] && pixels[1] == expected[1]);
        Require(SameRows(guest, reference, 3));
    }
}

void TestPostProcMB() {
    Rows rows(3);
    for (std::size_t row = 0; row < 2; ++row)
        for (auto& info : rows.storage[0][row]) info.mbTexture = 3;
    rows.rows[0][0][1].mbTexture = 0;
    rows.rows[0][1][1].mbTexture = 0;
    rows.rows[0][0][1].mbDc = 5;
    rows.rows[0][1][1].mbDc = 5;
    std::vector<std::int32_t> p0(512, 0);
    std::vector<std::int32_t> p1(512, 0);
    p1[256] = 8;
    postProcMB_nid_postfix(rows.rows, p0.data() + 256, p1.data() + 256, 1, 0, 0);
    Require(p1[256] == 4 && p0[256 + 48] == 4 && p0[256 + 32] == 0 && p1[256 + 16] == 0);
    Require(rows.rows[0][1][1].blockDc[0][0] == 4 && rows.rows[0][0][1].blockDc[3][0] == 4);
    ComparePostProcess(postProcMB_nid_postfix, JxrReferencePostProcMB_nid_no_patch);
    RequireInvalidArgument([&] { postProcMB_nid_postfix(rows.rows, nullptr, p1.data() + 256, 1, 0, 0); });
    rows.rows[1][0] = nullptr;
    RequireInvalidArgument([&] { postProcMB_nid_postfix(rows.rows, p0.data() + 256, p1.data() + 256, 1, 1, 0); });
}

void TestPostProcBlock() {
    ComparePostProcess(postProcBlock_nid_postfix, JxrReferencePostProcBlock_nid_no_patch);
    Rows rows(3);
    std::vector<std::int32_t> p0(512, 0);
    std::vector<std::int32_t> p1(512, 0);
    RequireInvalidArgument([&] { postProcBlock_nid_postfix(rows.rows, p0.data() + 256, nullptr, 1, 0, 0); });
    RequireInvalidArgument([&] { postProcBlock_nid_postfix(rows.rows, p0.data() + 256, p1.data() + 256, 1, 16, 0); });
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
    TestTermPostProc();
    TestSlide();
    TestUpdate();
    TestPostProcMB();
    TestPostProcBlock();
    TestInitPostProc();
    TestExifMetadata();
    return 0;
}
