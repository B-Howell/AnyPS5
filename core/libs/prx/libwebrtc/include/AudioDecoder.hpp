#pragma once

#include "prx/libwebrtc/include/VirtualCall.hpp"

#include <cstddef>
#include <cstdint>

enum class WebRtcSpeechType : int {
    Speech = 1,
    ComfortNoise = 2,
};

struct WebRtcAudioDecoder {
    const WebRtcVirtualFunction* vtable;
};

struct WebRtcInt16Buffer;

constexpr std::size_t WebRtcAudioDecoderPacketDurationSlot = 8;
constexpr std::size_t WebRtcAudioDecoderPacketDurationRedundantSlot = 9;
constexpr std::size_t WebRtcAudioDecoderChannelsSlot = 12;
constexpr std::size_t WebRtcAudioDecoderDecodeInternalSlot = 13;
constexpr std::size_t WebRtcAudioDecoderDecodeRedundantInternalSlot = 14;

using WebRtcAudioDecoderPacketDuration = int (APS5_VABI*)(const WebRtcAudioDecoder*, const std::uint8_t*, std::size_t);
using WebRtcAudioDecoderChannels = std::size_t (APS5_VABI*)(const WebRtcAudioDecoder*);
using WebRtcAudioDecoderDecodeInternal = int (APS5_VABI*)(WebRtcAudioDecoder*, const std::uint8_t*, std::size_t, int, std::int16_t*, WebRtcSpeechType*);
