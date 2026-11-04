#pragma once

#include "prx/libwebrtc/include/VirtualCall.hpp"

#include <cstddef>
#include <cstdint>

struct WebRtcAudioEncoder {
    const WebRtcVirtualFunction* vtable;
};

struct WebRtcOptionalUint32 {
    union {
        char nullState;
        std::uint32_t value;
    } storage;
    bool engaged;
};

struct WebRtcOptionalMilliseconds {
    union {
        char nullState;
        std::int64_t value;
    } storage;
    bool engaged;
};

struct WebRtcOptionalFloat {
    union {
        char nullState;
        float value;
    } storage;
    bool engaged;
};

struct WebRtcAnaStats {
    WebRtcOptionalUint32 bitrateActionCounter;
    WebRtcOptionalUint32 channelActionCounter;
    WebRtcOptionalUint32 dtxActionCounter;
    WebRtcOptionalUint32 fecActionCounter;
    WebRtcOptionalUint32 frameLengthIncreaseCounter;
    WebRtcOptionalUint32 frameLengthDecreaseCounter;
    WebRtcOptionalFloat uplinkPacketLossFraction;
};

struct WebRtcBitrateAllocationUpdate {
    std::int64_t targetBitrateBps;
    std::int64_t stableTargetBitrateBps;
    double packetLossRatio;
    std::int64_t roundTripTimeUs;
    std::int64_t bwePeriodUs;
    double cwndReduceRatio;
};

constexpr std::size_t WebRtcAudioEncoderOnReceivedUplinkBandwidthSlot = 21;

using WebRtcAudioEncoderOnReceivedUplinkBandwidth = void (APS5_VABI*)(WebRtcAudioEncoder*, int, WebRtcOptionalMilliseconds);

static_assert(sizeof(WebRtcOptionalUint32) == 8);
static_assert(offsetof(WebRtcOptionalUint32, engaged) == 4);
static_assert(sizeof(WebRtcOptionalFloat) == 8);
static_assert(sizeof(WebRtcAnaStats) == 56);
static_assert(offsetof(WebRtcAnaStats, uplinkPacketLossFraction) == 48);
static_assert(sizeof(WebRtcOptionalMilliseconds) == 16);
static_assert(offsetof(WebRtcOptionalMilliseconds, engaged) == 8);
static_assert(sizeof(WebRtcBitrateAllocationUpdate) == 48);
static_assert(offsetof(WebRtcBitrateAllocationUpdate, targetBitrateBps) == 0);
static_assert(offsetof(WebRtcBitrateAllocationUpdate, bwePeriodUs) == 32);
