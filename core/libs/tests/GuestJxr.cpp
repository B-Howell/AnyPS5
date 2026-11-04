#include "prx/libc/include/Api.hpp"
#include "prx/libSceJxr/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
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

void Require(bool value) { if (!value) std::abort(); }

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
int allocations = 0;
int failAllocation = 0;
std::size_t lastAllocationSize = 0;
std::vector<void*> allocated;
std::vector<void*> freed;

void* APS5_VABI Allocate(std::size_t size) {
    ++allocations;
    if (failAllocation != 0 && allocations == failAllocation) return nullptr;
    lastAllocationSize = size;
    void* pointer = std::malloc(size);
    allocated.push_back(pointer);
    return pointer;
}
void APS5_VABI Free(void* pointer) {
    if (pointer != nullptr) {
        lastFreed = pointer;
        freed.push_back(pointer);
        ++frees;
    }
    std::free(pointer);
}
void* APS5_VABI Reallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI Calloc(std::size_t count, std::size_t size) { return std::calloc(count, size); }
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

void ResetAllocations() {
    allocations = 0;
    failAllocation = 0;
    frees = 0;
    allocated.clear();
    freed.clear();
}

bool Bumpy(const JxrPostProcInfo& info) {
    if (info.mbTexture != 3) return false;
    for (const auto& row : info.blockTexture)
        for (auto texture : row)
            if (texture != 3) return false;
    return true;
}

bool Same(const JxrPostProcInfo& left, const JxrPostProcInfo& right) {
    return left.mbDc == right.mbDc && left.mbTexture == right.mbTexture &&
           std::memcmp(left.blockDc, right.blockDc, sizeof(left.blockDc)) == 0 &&
           std::memcmp(left.blockTexture, right.blockTexture, sizeof(left.blockTexture)) == 0;
}

std::uint32_t seed = 0x2545f491u;

std::int32_t Next(std::int32_t low, std::int32_t high) {
    seed = seed * 1664525u + 1013904223u;
    return low + static_cast<std::int32_t>((seed >> 8) % static_cast<std::uint32_t>(high - low + 1));
}

void TestInitAndTerm() {
    for (std::size_t channels = 1; channels <= 3; ++channels) {
        for (std::size_t width : {std::size_t{1}, std::size_t{2}, std::size_t{5}, std::size_t{17}}) {
            ResetAllocations();
            JxrPostProcInfo* rows[16][2]{};
            Require(initPostProc_nid_postfix(rows, width, channels) == 0);
            Require(allocations == static_cast<int>(channels * 2));
            Require(lastAllocationSize == (width + 2) * sizeof(JxrPostProcInfo));
            for (std::size_t channel = 0; channel < 16; ++channel) {
                for (std::size_t row = 0; row < 2; ++row) {
                    auto* info = rows[channel][row];
                    if (channel >= channels) {
                        Require(info == nullptr);
                        continue;
                    }
                    Require(info == static_cast<JxrPostProcInfo*>(allocated[channel * 2 + row]) + 1);
                    Require(Bumpy(info[-1]) && Bumpy(info[width]));
                }
            }
            termPostProc_nid_postfix(rows, channels);
            Require(frees == static_cast<int>(channels * 2));
            for (std::size_t index = 0; index < freed.size(); ++index) Require(freed[index] == allocated[index]);
        }
    }

    ResetAllocations();
    JxrPostProcInfo* empty[16][2]{};
    Require(initPostProc_nid_postfix(empty, 4, 0) == 0);
    Require(initPostProc_nid_postfix(nullptr, 4, 0) == 0);
    termPostProc_nid_postfix(nullptr, 0);
    Require(allocations == 0 && frees == 0);

    JxrPostProcInfo* wide[16][2]{};
    Require(initPostProc_nid_postfix(wide, (std::size_t{745} << 16) - 2, 1) == -1);
    Require(allocations == 0 && wide[0][0] == nullptr);
    RequireInvalidArgument([&] { initPostProc_nid_postfix(wide, std::numeric_limits<std::size_t>::max(), 1); });
    RequireInvalidArgument([] { initPostProc_nid_postfix(nullptr, 4, 1); });
    RequireInvalidArgument([&] { initPostProc_nid_postfix(wide, 4, 17); });
    Require(allocations == 0);

    ResetAllocations();
    failAllocation = 3;
    JxrPostProcInfo* failed[16][2]{};
    Require(initPostProc_nid_postfix(failed, 4, 2) == -1);
    Require(failed[0][0] != nullptr && failed[0][1] != nullptr && failed[1][0] == nullptr && failed[1][1] == nullptr);
    termPostProc_nid_postfix(failed, 2);
    Require(frees == 2 && freed[0] == allocated[0] && freed[1] == allocated[1]);
}

struct Rows {
    std::vector<JxrPostProcInfo> storage[2][2];
    JxrPostProcInfo* rows[16][2]{};

    explicit Rows(std::size_t width) {
        for (std::size_t channel = 0; channel < 2; ++channel) {
            for (std::size_t row = 0; row < 2; ++row) {
                storage[channel][row].assign(width + 2, JxrPostProcInfo{});
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
            JxrPostProcInfo* previous[2][2]{};
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
    void* const allocation = memory;
    Require(PKFree_nid_postfix(&memory) == 0);
    Require(memory == nullptr);
    Require(frees == 1 && lastFreed == allocation);
    Require(PKFree_nid_postfix(&memory) == 0);
    Require(memory == nullptr);
    Require(frees == 1);
    Require(PKFree_nid_postfix(nullptr) == 0);
    Require(frees == 1);
    TestInitAndTerm();
    TestSlide();
    TestUpdate();
    TestPostProcMB();
    TestPostProcBlock();
    return 0;
}
