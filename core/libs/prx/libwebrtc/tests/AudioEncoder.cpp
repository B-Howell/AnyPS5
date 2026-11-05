#include "prx/libwebrtc/libwebrtc.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <cstring>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc AudioEncoder check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> void RequireInvalidArgument(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, location);
}

void Fill(WebRtcAnaStats& stats) {
    std::memset(&stats, 0x5A, sizeof(stats));
}

bool Cleared(const WebRtcOptionalUint32& optional) {
    const auto* bytes = reinterpret_cast<const unsigned char*>(&optional);
    return !optional.engaged && bytes[0] == 0 && bytes[1] == 0x5A && bytes[2] == 0x5A && bytes[3] == 0x5A && bytes[5] == 0x5A && bytes[6] == 0x5A && bytes[7] == 0x5A;
}

bool AllCleared(const WebRtcAnaStats& stats) {
    WebRtcOptionalUint32 loss;
    std::memcpy(&loss, &stats.uplinkPacketLossFraction, sizeof(loss));
    return Cleared(stats.bitrateActionCounter) && Cleared(stats.channelActionCounter) && Cleared(stats.dtxActionCounter) && Cleared(stats.fecActionCounter) &&
        Cleared(stats.frameLengthIncreaseCounter) && Cleared(stats.frameLengthDecreaseCounter) && Cleared(loss);
}

WebRtcAudioEncoder* Encoder() {
    static unsigned char storage[64];
    std::memset(storage, 0xC3, sizeof(storage));
    return reinterpret_cast<WebRtcAudioEncoder*>(storage);
}

bool EncoderUnchanged(const WebRtcAudioEncoder* encoder) {
    const auto* bytes = reinterpret_cast<const unsigned char*>(encoder);
    for (int i = 0; i < 64; ++i)
        if (bytes[i] != 0xC3) return false;
    return true;
}

void TestAnaStats() {
    WebRtcAnaStats stats;
    Fill(stats);
    _ZN6webrtc8ANAStatsC1Ev_nid_postfix(&stats);
    Require(AllCleared(stats));

    WebRtcAudioEncoder* encoder = Encoder();
    WebRtcAnaStats result;
    Fill(result);
    Require(_ZNK6webrtc12AudioEncoder11GetANAStatsEv_nid_postfix(&result, encoder) == &result);
    Require(AllCleared(result));
    Require(EncoderUnchanged(encoder));
}

void TestDefaults() {
    WebRtcAudioEncoder* encoder = Encoder();
    Require(!_ZNK6webrtc12AudioEncoder6GetDtxEv_nid_postfix(encoder));
    _ZN6webrtc12AudioEncoder16SetTargetBitrateEi_nid_postfix(encoder, 32000);
    _ZN6webrtc12AudioEncoder26DisableAudioNetworkAdaptorEv_nid_postfix(encoder);
    _ZN6webrtc12AudioEncoder13OnReceivedRttEi_nid_postfix(encoder, 120);
    _ZN6webrtc12AudioEncoder18OnReceivedOverheadEm_nid_postfix(encoder, 40);
    _ZN6webrtc12AudioEncoder27SetReceiverFrameLengthRangeEii_nid_postfix(encoder, 20, 120);
    Require(!_ZNK6webrtc12AudioEncoder6GetDtxEv_nid_postfix(encoder));
    Require(EncoderUnchanged(encoder));
}

void TestNullPointers() {
    RequireInvalidArgument([] { _ZN6webrtc8ANAStatsC1Ev_nid_postfix(nullptr); });
    RequireInvalidArgument([] { _ZNK6webrtc12AudioEncoder11GetANAStatsEv_nid_postfix(nullptr, Encoder()); });
}

struct BandwidthCall {
    int count;
    WebRtcAudioEncoder* encoder;
    int targetAudioBitrateBps;
    WebRtcOptionalMilliseconds bwePeriodMs;
};

BandwidthCall bandwidth;

void APS5_VABI OnReceivedUplinkBandwidth(WebRtcAudioEncoder* encoder, int targetAudioBitrateBps, WebRtcOptionalMilliseconds bwePeriodMs) {
    ++bandwidth.count;
    bandwidth.encoder = encoder;
    bandwidth.targetAudioBitrateBps = targetAudioBitrateBps;
    bandwidth.bwePeriodMs = bwePeriodMs;
}

constexpr std::size_t FixtureOnReceivedUplinkBandwidthSlot = 21;

WebRtcVirtualFunction encoderVtable[22];

WebRtcAudioEncoder MakeEncoder() {
    for (WebRtcVirtualFunction& slot : encoderVtable)
        slot = nullptr;
    encoderVtable[FixtureOnReceivedUplinkBandwidthSlot] = reinterpret_cast<WebRtcVirtualFunction>(&OnReceivedUplinkBandwidth);
    bandwidth = {};
    std::memset(&bandwidth.bwePeriodMs, 0x5A, sizeof(bandwidth.bwePeriodMs));
    return {encoderVtable};
}

WebRtcBitrateAllocationUpdate Update(std::int64_t targetBitrateBps, std::int64_t bwePeriodUs) {
    WebRtcBitrateAllocationUpdate update{};
    update.targetBitrateBps = targetBitrateBps;
    update.stableTargetBitrateBps = 1111;
    update.packetLossRatio = 0.25;
    update.roundTripTimeUs = std::numeric_limits<std::int64_t>::max();
    update.bwePeriodUs = bwePeriodUs;
    update.cwndReduceRatio = 0.5;
    return update;
}

bool Received(const WebRtcAudioEncoder& encoder, int targetAudioBitrateBps, std::int64_t bwePeriodMs) {
    return bandwidth.count == 1 && bandwidth.encoder == &encoder && bandwidth.targetAudioBitrateBps == targetAudioBitrateBps && bandwidth.bwePeriodMs.engaged &&
        bandwidth.bwePeriodMs.storage.value == bwePeriodMs;
}

void TestTargetAudioBitrate() {
    WebRtcAudioEncoder encoder = MakeEncoder();
    _ZN6webrtc12AudioEncoder28OnReceivedTargetAudioBitrateEi_nid_postfix(&encoder, 32000);
    Require(bandwidth.count == 1 && bandwidth.encoder == &encoder && bandwidth.targetAudioBitrateBps == 32000);
    Require(!bandwidth.bwePeriodMs.engaged && bandwidth.bwePeriodMs.storage.nullState == 0);

    encoder = MakeEncoder();
    _ZN6webrtc12AudioEncoder28OnReceivedTargetAudioBitrateEi_nid_postfix(&encoder, -5);
    Require(bandwidth.count == 1 && bandwidth.targetAudioBitrateBps == -5 && !bandwidth.bwePeriodMs.engaged);
}

void Allocate(WebRtcAudioEncoder& encoder, std::int64_t targetBitrateBps, std::int64_t bwePeriodUs) {
    encoder = MakeEncoder();
    _ZN6webrtc12AudioEncoder26OnReceivedUplinkAllocationENS_23BitrateAllocationUpdateE_nid_postfix(&encoder, Update(targetBitrateBps, bwePeriodUs));
}

void TestUplinkAllocation() {
    constexpr std::int64_t plusInfinity = std::numeric_limits<std::int64_t>::max();
    constexpr std::int64_t minusInfinity = std::numeric_limits<std::int64_t>::min();
    WebRtcAudioEncoder encoder{};
    Allocate(encoder, 64000, 2500499);
    Require(Received(encoder, 64000, 2500));
    Allocate(encoder, 64000, 2500500);
    Require(Received(encoder, 64000, 2501));
    Allocate(encoder, 64000, -2500500);
    Require(Received(encoder, 64000, -2500));
    Allocate(encoder, 64000, -2500501);
    Require(Received(encoder, 64000, -2501));
    Allocate(encoder, 64000, plusInfinity);
    Require(Received(encoder, 64000, 9223372036854776));
    Allocate(encoder, 64000, minusInfinity);
    Require(Received(encoder, 64000, -9223372036854776));
    Allocate(encoder, 0x100007D00, 0);
    Require(Received(encoder, 32000, 0));
}

void TestUplinkAllocationTypedGuestAbiCall() {
    using AllocationCall = void (APS5_VABI*)(WebRtcAudioEncoder*, WebRtcBitrateAllocationUpdate);
    AllocationCall allocation = &_ZN6webrtc12AudioEncoder26OnReceivedUplinkAllocationENS_23BitrateAllocationUpdateE_nid_postfix;
    WebRtcAudioEncoder encoder = MakeEncoder();
    const WebRtcBitrateAllocationUpdate update = Update(48000, 7000);
    allocation(&encoder, update);
    Require(Received(encoder, 48000, 7));
}

void TestNullEncoder() {
    RequireInvalidArgument([] { _ZN6webrtc12AudioEncoder28OnReceivedTargetAudioBitrateEi_nid_postfix(nullptr, 32000); });
    RequireInvalidArgument([] { _ZN6webrtc12AudioEncoder26OnReceivedUplinkAllocationENS_23BitrateAllocationUpdateE_nid_postfix(nullptr, Update(32000, 1000)); });
}

}

int main() {
    TestAnaStats();
    TestDefaults();
    TestNullPointers();
    TestTargetAudioBitrate();
    TestUplinkAllocation();
    TestUplinkAllocationTypedGuestAbiCall();
    TestNullEncoder();
    return 0;
}
