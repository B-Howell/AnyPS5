#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>

using WebRtcVirtualFunction = void (APS5_VABI*)();

template<typename TFunction, typename TObject> TFunction WebRtcVirtualSlot(const TObject* object, std::size_t slot) {
    return reinterpret_cast<TFunction>(object->vtable[slot]);
}
