#include "prx/libwebrtc/libwebrtc.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc AudioDecoder check failed at line %u\n", location.line());
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

unsigned char decoderStorage[64];
unsigned char bufferStorage[32];
std::int16_t samples[64];

WebRtcAudioDecoder* Decoder() {
    std::memset(decoderStorage, 0xC3, sizeof(decoderStorage));
    std::memset(bufferStorage, 0xA5, sizeof(bufferStorage));
    for (std::int16_t& sample : samples)
        sample = 0x1234;
    return reinterpret_cast<WebRtcAudioDecoder*>(decoderStorage);
}

bool Unchanged() {
    for (unsigned char byte : decoderStorage)
        if (byte != 0xC3) return false;
    for (unsigned char byte : bufferStorage)
        if (byte != 0xA5) return false;
    for (std::int16_t sample : samples)
        if (sample != 0x1234) return false;
    return true;
}

void TestPlc() {
    WebRtcAudioDecoder* decoder = Decoder();
    Require(!_ZNK6webrtc12AudioDecoder12HasDecodePlcEv_nid_postfix(decoder));
    Require(_ZN6webrtc12AudioDecoder9DecodePlcEmPs_nid_postfix(decoder, 0, samples) == 0);
    Require(_ZN6webrtc12AudioDecoder9DecodePlcEmPs_nid_postfix(decoder, 3, samples) == 0);
    Require(_ZN6webrtc12AudioDecoder9DecodePlcEmPs_nid_postfix(decoder, 1, nullptr) == 0);
    _ZN6webrtc12AudioDecoder11GeneratePlcEmPN3rtc7BufferTIsLb0EEE_nid_postfix(decoder, 480, reinterpret_cast<WebRtcInt16Buffer*>(bufferStorage));
    _ZN6webrtc12AudioDecoder11GeneratePlcEmPN3rtc7BufferTIsLb0EEE_nid_postfix(decoder, 480, nullptr);
    Require(Unchanged());
}

void TestErrorCode() {
    WebRtcAudioDecoder* decoder = Decoder();
    Require(_ZN6webrtc12AudioDecoder9ErrorCodeEv_nid_postfix(decoder) == 0);
    Require(Unchanged());
}

struct FakeDecoder {
    WebRtcAudioDecoder base;
    int duration;
    int redundantDuration;
    std::size_t channels;
};

struct DecoderCalls {
    int packetDuration;
    int packetDurationRedundant;
    int channels;
    int decodeInternal;
    int decodeRedundantInternal;
    const WebRtcAudioDecoder* self;
    const std::uint8_t* encoded;
    std::size_t encodedLen;
    int sampleRateHz;
    std::int16_t* decoded;
    WebRtcSpeechType* speechType;
};

DecoderCalls calls;

const FakeDecoder* Fake(const WebRtcAudioDecoder* decoder) {
    return reinterpret_cast<const FakeDecoder*>(decoder);
}

void RecordDuration(const WebRtcAudioDecoder* decoder, const std::uint8_t* encoded, std::size_t encodedLen) {
    calls.self = decoder;
    calls.encoded = encoded;
    calls.encodedLen = encodedLen;
}

int APS5_VABI PacketDuration(const WebRtcAudioDecoder* decoder, const std::uint8_t* encoded, std::size_t encodedLen) {
    ++calls.packetDuration;
    RecordDuration(decoder, encoded, encodedLen);
    return Fake(decoder)->duration;
}

int APS5_VABI PacketDurationRedundant(const WebRtcAudioDecoder* decoder, const std::uint8_t* encoded, std::size_t encodedLen) {
    ++calls.packetDurationRedundant;
    RecordDuration(decoder, encoded, encodedLen);
    return Fake(decoder)->redundantDuration;
}

std::size_t APS5_VABI Channels(const WebRtcAudioDecoder* decoder) {
    ++calls.channels;
    return Fake(decoder)->channels;
}

void RecordDecode(WebRtcAudioDecoder* decoder, const std::uint8_t* encoded, std::size_t encodedLen, int sampleRateHz, std::int16_t* decoded, WebRtcSpeechType* speechType) {
    calls.self = decoder;
    calls.encoded = encoded;
    calls.encodedLen = encodedLen;
    calls.sampleRateHz = sampleRateHz;
    calls.decoded = decoded;
    calls.speechType = speechType;
}

int APS5_VABI DecodeInternal(WebRtcAudioDecoder* decoder, const std::uint8_t* encoded, std::size_t encodedLen, int sampleRateHz, std::int16_t* decoded, WebRtcSpeechType* speechType) {
    ++calls.decodeInternal;
    RecordDecode(decoder, encoded, encodedLen, sampleRateHz, decoded, speechType);
    return 77;
}

int APS5_VABI DecodeRedundantInternal(WebRtcAudioDecoder* decoder, const std::uint8_t* encoded, std::size_t encodedLen, int sampleRateHz, std::int16_t* decoded,
    WebRtcSpeechType* speechType) {
    ++calls.decodeRedundantInternal;
    RecordDecode(decoder, encoded, encodedLen, sampleRateHz, decoded, speechType);
    return 88;
}

WebRtcVirtualFunction decoderVtable[15];

FakeDecoder MakeDecoder(int duration, int redundantDuration, std::size_t channels) {
    for (WebRtcVirtualFunction& slot : decoderVtable)
        slot = nullptr;
    decoderVtable[WebRtcAudioDecoderPacketDurationSlot] = reinterpret_cast<WebRtcVirtualFunction>(&PacketDuration);
    decoderVtable[WebRtcAudioDecoderPacketDurationRedundantSlot] = reinterpret_cast<WebRtcVirtualFunction>(&PacketDurationRedundant);
    decoderVtable[WebRtcAudioDecoderChannelsSlot] = reinterpret_cast<WebRtcVirtualFunction>(&Channels);
    decoderVtable[WebRtcAudioDecoderDecodeInternalSlot] = reinterpret_cast<WebRtcVirtualFunction>(&DecodeInternal);
    decoderVtable[WebRtcAudioDecoderDecodeRedundantInternalSlot] = reinterpret_cast<WebRtcVirtualFunction>(&DecodeRedundantInternal);
    calls = {};
    return {{decoderVtable}, duration, redundantDuration, channels};
}

const std::uint8_t payload[5] = {1, 2, 3, 4, 5};
WebRtcSpeechType speech = WebRtcSpeechType::Speech;

int Decode(FakeDecoder& decoder, std::size_t maxDecodedBytes) {
    return _ZN6webrtc12AudioDecoder6DecodeEPKhmimPsPNS0_10SpeechTypeE_nid_postfix(&decoder.base, payload, sizeof(payload), 48000, maxDecodedBytes, samples, &speech);
}

int DecodeRedundant(FakeDecoder& decoder, std::size_t maxDecodedBytes) {
    return _ZN6webrtc12AudioDecoder15DecodeRedundantEPKhmimPsPNS0_10SpeechTypeE_nid_postfix(&decoder.base, payload, sizeof(payload), 16000, maxDecodedBytes, samples, &speech);
}

bool Forwarded(const FakeDecoder& decoder, int sampleRateHz) {
    return calls.self == &decoder.base && calls.encoded == payload && calls.encodedLen == sizeof(payload) && calls.sampleRateHz == sampleRateHz && calls.decoded == samples &&
        calls.speechType == &speech;
}

void TestDecodeUnknownDuration() {
    FakeDecoder decoder = MakeDecoder(-1, 1000, 2);
    Require(Decode(decoder, 0) == 77);
    Require(calls.packetDuration == 1 && calls.packetDurationRedundant == 0 && calls.channels == 0);
    Require(calls.decodeInternal == 1 && calls.decodeRedundantInternal == 0);
    Require(Forwarded(decoder, 48000));
}

void TestDecodeWithinLimit() {
    FakeDecoder decoder = MakeDecoder(480, -1, 2);
    Require(Decode(decoder, 1920) == 77);
    Require(calls.packetDuration == 1 && calls.channels == 1 && calls.decodeInternal == 1);
    Require(Forwarded(decoder, 48000));

    decoder = MakeDecoder(0, -1, 2);
    Require(Decode(decoder, 0) == 77);
    Require(calls.decodeInternal == 1);
}

void TestDecodeOverLimit() {
    FakeDecoder decoder = MakeDecoder(480, -1, 2);
    Require(Decode(decoder, 1919) == -1);
    Require(calls.packetDuration == 1 && calls.channels == 1);
    Require(calls.decodeInternal == 0 && calls.decodeRedundantInternal == 0);
    Require(calls.self == &decoder.base && calls.encoded == payload && calls.encodedLen == sizeof(payload));
}

void TestDecodeRedundant() {
    FakeDecoder decoder = MakeDecoder(100000, 160, 1);
    Require(DecodeRedundant(decoder, 320) == 88);
    Require(calls.packetDuration == 0 && calls.packetDurationRedundant == 1 && calls.channels == 1);
    Require(calls.decodeInternal == 0 && calls.decodeRedundantInternal == 1);
    Require(Forwarded(decoder, 16000));

    decoder = MakeDecoder(0, 160, 1);
    Require(DecodeRedundant(decoder, 319) == -1);
    Require(calls.packetDurationRedundant == 1 && calls.decodeRedundantInternal == 0 && calls.decodeInternal == 0);

    decoder = MakeDecoder(100000, -1, 1);
    Require(DecodeRedundant(decoder, 0) == 88);
    Require(calls.channels == 0 && calls.decodeRedundantInternal == 1);
}

void TestDecodeNullDecoder() {
    RequireInvalidArgument([] { _ZN6webrtc12AudioDecoder6DecodeEPKhmimPsPNS0_10SpeechTypeE_nid_postfix(nullptr, payload, sizeof(payload), 48000, 0, samples, &speech); });
    RequireInvalidArgument([] { _ZN6webrtc12AudioDecoder15DecodeRedundantEPKhmimPsPNS0_10SpeechTypeE_nid_postfix(nullptr, payload, sizeof(payload), 48000, 0, samples, &speech); });
}

}

int main() {
    TestPlc();
    TestErrorCode();
    TestDecodeUnknownDuration();
    TestDecodeWithinLimit();
    TestDecodeOverLimit();
    TestDecodeRedundant();
    TestDecodeNullDecoder();
    return 0;
}
