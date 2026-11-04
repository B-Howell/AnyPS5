#include "prx/libSceJxr/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

extern "C" {
void JxrSlideOneMBRow_nid_no_patch(void* rows, std::size_t channels, std::size_t mbWidth, int top, int bottom);
void JxrUpdatePostProcInfo_nid_no_patch(void* rows, std::int32_t* mb, std::size_t mbX, std::size_t channel);
void JxrPostProcMB_nid_no_patch(void* rows, std::int32_t* p0, std::int32_t* p1, std::size_t mbX, std::size_t channel,
                                int threshold);
void JxrPostProcBlock_nid_no_patch(void* rows, std::int32_t* p0, std::int32_t* p1, std::size_t mbX, std::size_t channel,
                                   int threshold);
}

namespace {

constexpr std::size_t MaxChannels = 16;
constexpr std::int32_t IcerrOk = 0;
constexpr std::int32_t IcerrError = -1;
constexpr std::uint8_t Bumpy = 3;

static_assert(sizeof(JxrPostProcInfo) == 88 && alignof(JxrPostProcInfo) == 4);
static_assert(offsetof(JxrPostProcInfo, mbTexture) == 4 && offsetof(JxrPostProcInfo, blockDc) == 8 &&
              offsetof(JxrPostProcInfo, blockTexture) == 72);

void RequireRows(JxrPostProcRows rows, std::size_t channels) {
    if (channels > MaxChannels || (channels != 0 && rows == nullptr)) APS5_INVALID_ARG_EX;
}

void RequireChannel(JxrPostProcRows rows, std::size_t channel, bool previous) {
    if (rows == nullptr || channel >= MaxChannels || rows[channel][1] == nullptr) APS5_INVALID_ARG_EX;
    if (previous && rows[channel][0] == nullptr) APS5_INVALID_ARG_EX;
}

}

extern "C" {

std::int32_t APS5_VABI initPostProc_nid_postfix(JxrPostProcRows rows, std::size_t mbWidth, std::size_t channels) {
    RequireRows(rows, channels);
    for (std::size_t channel = 0; channel < channels; ++channel) {
        for (std::size_t row = 0; row < 2; ++row) {
            if ((((mbWidth + 2) >> 16) * sizeof(JxrPostProcInfo)) & std::size_t{0xffff0000u}) return IcerrError;
            std::size_t count = 0;
            std::size_t bytes = 0;
            if (__builtin_add_overflow(mbWidth, std::size_t{2}, &count) ||
                __builtin_mul_overflow(count, sizeof(JxrPostProcInfo), &bytes))
                APS5_INVALID_ARG_EX;
            auto* block = static_cast<JxrPostProcInfo*>(malloc_nid_postfix(bytes));
            rows[channel][row] = block;
            if (block == nullptr) return IcerrError;
            auto* info = block + 1;
            rows[channel][row] = info;
            info[-1].mbTexture = Bumpy;
            std::memset(info[-1].blockTexture, Bumpy, sizeof(info[-1].blockTexture));
            std::memcpy(&info[mbWidth], &info[-1], sizeof(JxrPostProcInfo));
        }
    }
    return IcerrOk;
}

void APS5_VABI termPostProc_nid_postfix(JxrPostProcRows rows, std::size_t channels) {
    RequireRows(rows, channels);
    for (std::size_t channel = 0; channel < channels; ++channel) {
        for (std::size_t row = 0; row < 2; ++row) {
            if (rows[channel][row] != nullptr) free_nid_postfix(rows[channel][row] - 1);
        }
    }
}

void APS5_VABI slideOneMBRow_nid_postfix(JxrPostProcRows rows, std::size_t channels, std::size_t mbWidth, int top,
                                         int bottom) {
    RequireRows(rows, channels);
    for (std::size_t channel = 0; channel < channels && mbWidth != 0; ++channel) {
        if ((top && rows[channel][1] == nullptr) || (bottom && rows[channel][0] == nullptr)) APS5_INVALID_ARG_EX;
    }
    JxrSlideOneMBRow_nid_no_patch(rows, channels, mbWidth, top, bottom);
}

void APS5_VABI updatePostProcInfo_nid_postfix(JxrPostProcRows rows, std::int32_t* mb, std::size_t mbX,
                                              std::size_t channel) {
    RequireChannel(rows, channel, false);
    if (mb == nullptr) APS5_INVALID_ARG_EX;
    JxrUpdatePostProcInfo_nid_no_patch(rows, mb, mbX, channel);
}

void APS5_VABI postProcMB_nid_postfix(JxrPostProcRows rows, std::int32_t* p0, std::int32_t* p1, std::size_t mbX,
                                      std::size_t channel, int threshold) {
    RequireChannel(rows, channel, true);
    if (p0 == nullptr || p1 == nullptr) APS5_INVALID_ARG_EX;
    JxrPostProcMB_nid_no_patch(rows, p0, p1, mbX, channel, threshold);
}

void APS5_VABI postProcBlock_nid_postfix(JxrPostProcRows rows, std::int32_t* p0, std::int32_t* p1, std::size_t mbX,
                                         std::size_t channel, int threshold) {
    RequireChannel(rows, channel, true);
    if (p0 == nullptr || p1 == nullptr) APS5_INVALID_ARG_EX;
    JxrPostProcBlock_nid_no_patch(rows, p0, p1, mbX, channel, threshold);
}

}
