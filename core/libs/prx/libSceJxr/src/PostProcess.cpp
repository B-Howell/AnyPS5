#include "prx/libSceJxr/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>

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

static_assert(sizeof(JxrGuestPostProcInfo) == 88 && alignof(JxrGuestPostProcInfo) == 4);
static_assert(offsetof(JxrGuestPostProcInfo, mbTexture) == 4 && offsetof(JxrGuestPostProcInfo, blockDc) == 8 &&
              offsetof(JxrGuestPostProcInfo, blockTexture) == 72);

void RequireRows(JxrPostProcRows rows, std::size_t channels) {
    if (channels > MaxChannels || (channels != 0 && rows == nullptr)) APS5_INVALID_ARG_EX;
}

void RequireChannel(JxrPostProcRows rows, std::size_t channel, bool previous) {
    if (rows == nullptr || channel >= MaxChannels || rows[channel][1] == nullptr) APS5_INVALID_ARG_EX;
    if (previous && rows[channel][0] == nullptr) APS5_INVALID_ARG_EX;
}

}

extern "C" {

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
