/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
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
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

void* VirtualFunction(const WebRtcVideoFrameBuffer* buffer, WebRtcVideoFrameBufferSlot slot, const char* name) {
    if (!buffer)
        throw std::invalid_argument(std::string(name) + " requires a buffer");
    const void* const* vtable;
    std::memcpy(&vtable, buffer, sizeof(vtable));
    const auto index = static_cast<std::size_t>(slot);
    if (!vtable || !vtable[index])
        throw std::invalid_argument(std::string(name) + " requires a buffer virtual function");
    return const_cast<void*>(vtable[index]);
}

int ChromaSize(const WebRtcVideoFrameBuffer* buffer, WebRtcVideoFrameBufferSlot slot, const char* name) {
    using Size = int(APS5_VABI*)(const WebRtcVideoFrameBuffer*);
    const int size = reinterpret_cast<Size>(VirtualFunction(buffer, slot, name))(buffer);
    if (size == std::numeric_limits<int>::max())
        throw std::overflow_error(std::string(name) + " overflows int for a plane size of INT_MAX");
    return (size + 1) / 2;
}

void AddRef(const WebRtcVideoFrameBuffer* buffer, const char* name) {
    using AddRefFunction = void(APS5_VABI*)(const WebRtcVideoFrameBuffer*);
    reinterpret_cast<AddRefFunction>(VirtualFunction(buffer, WebRtcVideoFrameBufferSlot::AddRef, name))(buffer);
}

WebRtcVideoFrameBufferType Type(const WebRtcVideoFrameBuffer* buffer, const char* name) {
    using TypeFunction = WebRtcVideoFrameBufferType(APS5_VABI*)(const WebRtcVideoFrameBuffer*);
    return reinterpret_cast<TypeFunction>(VirtualFunction(buffer, WebRtcVideoFrameBufferSlot::Type, name))(buffer);
}

}

extern "C" {

int APS5_VABI _ZNK6webrtc19I420BufferInterface11ChromaWidthEv_nid_postfix(const WebRtcVideoFrameBuffer* buffer) {
    return ChromaSize(buffer, WebRtcVideoFrameBufferSlot::Width, "I420BufferInterface::ChromaWidth");
}

int APS5_VABI _ZNK6webrtc19I420BufferInterface12ChromaHeightEv_nid_postfix(const WebRtcVideoFrameBuffer* buffer) {
    return ChromaSize(buffer, WebRtcVideoFrameBufferSlot::Height, "I420BufferInterface::ChromaHeight");
}

WebRtcVideoFrameBufferRef* APS5_VABI _ZN6webrtc19I420BufferInterface6ToI420Ev_nid_postfix(WebRtcVideoFrameBufferRef* result, WebRtcVideoFrameBuffer* buffer) {
    if (!result)
        throw std::invalid_argument("I420BufferInterface::ToI420 requires a result");
    if (buffer)
        AddRef(buffer, "I420BufferInterface::ToI420");
    result->buffer = buffer;
    return result;
}

const WebRtcVideoFrameBuffer* APS5_VABI _ZNK6webrtc19I420BufferInterface7GetI420Ev_nid_postfix(const WebRtcVideoFrameBuffer* buffer) {
    return buffer;
}

WebRtcVideoFrameBufferRef* APS5_VABI _ZN6webrtc16VideoFrameBuffer20GetMappedFrameBufferEN3rtc9ArrayViewINS0_4TypeELln4711EEE_nid_postfix(
    WebRtcVideoFrameBufferRef* result, WebRtcVideoFrameBuffer* buffer, WebRtcVideoFrameBufferType*, std::size_t) {
    if (!result)
        throw std::invalid_argument("VideoFrameBuffer::GetMappedFrameBuffer requires a result");
    if (Type(buffer, "VideoFrameBuffer::GetMappedFrameBuffer") != WebRtcVideoFrameBufferType::Native)
        throw std::invalid_argument("VideoFrameBuffer::GetMappedFrameBuffer requires a kNative buffer");
    result->buffer = nullptr;
    return result;
}

}
