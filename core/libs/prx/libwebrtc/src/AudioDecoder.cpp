/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * Copyright (c) 2011, The WebRTC project authors. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *
 *   * Neither the name of Google nor the names of its contributors may
 *     be used to endorse or promote products derived from this software
 *     without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libwebrtc/libwebrtc.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace {

int DecodeChecked(WebRtcAudioDecoder* decoder, std::size_t durationSlot, std::size_t decodeSlot, const std::uint8_t* encoded, std::size_t encodedLen, int sampleRateHz,
    std::size_t maxDecodedBytes, std::int16_t* decoded, WebRtcSpeechType* speechType) {
    if (!decoder)
        throw std::invalid_argument("AudioDecoder::Decode requires a decoder");
    int duration = WebRtcVirtualSlot<WebRtcAudioDecoderPacketDuration>(decoder, durationSlot)(decoder, encoded, encodedLen);
    if (duration >= 0 && duration * WebRtcVirtualSlot<WebRtcAudioDecoderChannels>(decoder, WebRtcAudioDecoderChannelsSlot)(decoder) * sizeof(std::int16_t) > maxDecodedBytes)
        return -1;
    return WebRtcVirtualSlot<WebRtcAudioDecoderDecodeInternal>(decoder, decodeSlot)(decoder, encoded, encodedLen, sampleRateHz, decoded, speechType);
}

}

extern "C" {

int APS5_VABI _ZN6webrtc12AudioDecoder6DecodeEPKhmimPsPNS0_10SpeechTypeE_nid_postfix(WebRtcAudioDecoder* decoder, const std::uint8_t* encoded, std::size_t encodedLen, int sampleRateHz,
    std::size_t maxDecodedBytes, std::int16_t* decoded, WebRtcSpeechType* speechType) {
    return DecodeChecked(decoder, WebRtcAudioDecoderPacketDurationSlot, WebRtcAudioDecoderDecodeInternalSlot, encoded, encodedLen, sampleRateHz, maxDecodedBytes, decoded, speechType);
}

int APS5_VABI _ZN6webrtc12AudioDecoder15DecodeRedundantEPKhmimPsPNS0_10SpeechTypeE_nid_postfix(WebRtcAudioDecoder* decoder, const std::uint8_t* encoded, std::size_t encodedLen,
    int sampleRateHz, std::size_t maxDecodedBytes, std::int16_t* decoded, WebRtcSpeechType* speechType) {
    return DecodeChecked(decoder, WebRtcAudioDecoderPacketDurationRedundantSlot, WebRtcAudioDecoderDecodeRedundantInternalSlot, encoded, encodedLen, sampleRateHz, maxDecodedBytes, decoded,
        speechType);
}

bool APS5_VABI _ZNK6webrtc12AudioDecoder12HasDecodePlcEv_nid_postfix(const WebRtcAudioDecoder*) {
    return false;
}

std::size_t APS5_VABI _ZN6webrtc12AudioDecoder9DecodePlcEmPs_nid_postfix(WebRtcAudioDecoder*, std::size_t, std::int16_t*) {
    return 0;
}

void APS5_VABI _ZN6webrtc12AudioDecoder11GeneratePlcEmPN3rtc7BufferTIsLb0EEE_nid_postfix(WebRtcAudioDecoder*, std::size_t, WebRtcInt16Buffer*) {}

int APS5_VABI _ZN6webrtc12AudioDecoder9ErrorCodeEv_nid_postfix(WebRtcAudioDecoder*) {
    return 0;
}

}
