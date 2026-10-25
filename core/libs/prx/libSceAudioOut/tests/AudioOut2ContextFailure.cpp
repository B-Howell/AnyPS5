#define SDL_MAIN_HANDLED
#include <array>
#include <cstdio>
#include <cstdlib>
#include <new>
#include "prx/libSceAudioOut/include/AudioOut2.hpp"
#include "SDL.h"

static bool armed = false;
static std::size_t failAt = 0;
static std::size_t attempts = 0;
static std::array<void*, 128> allocations{};

void* operator new(std::size_t size) {
    if (armed && attempts++ == failAt) throw std::bad_alloc{};
    void* pointer = std::malloc(size ? size : 1);
    if (!pointer) throw std::bad_alloc{};
    if (armed) {
        for (auto& slot : allocations) {
            if (!slot) { slot = pointer; return pointer; }
        }
        std::abort();
    }
    return pointer;
}
void operator delete(void* pointer) noexcept {
    for (auto& slot : allocations) if (slot == pointer) slot = nullptr;
    std::free(pointer);
}
void operator delete(void* pointer, std::size_t) noexcept { ::operator delete(pointer); }
int InitializeSdlAudio_nid_no_patch() { return -1; }
extern "C" const char* SDLCALL SDL_GetError() { return "test: no audio device"; }
extern "C" SDL_AudioDeviceID SDLCALL SDL_OpenAudioDevice(const char*, int, const SDL_AudioSpec*, SDL_AudioSpec*, int) { std::abort(); }
extern "C" void SDLCALL SDL_PauseAudioDevice(SDL_AudioDeviceID, int) { std::abort(); }
extern "C" Uint32 SDLCALL SDL_WasInit(Uint32) { return 0; }
extern "C" void SDLCALL SDL_CloseAudioDevice(SDL_AudioDeviceID) { std::abort(); }
extern "C" int SDLCALL SDL_GetNumAudioDevices(int) { return 0; }
extern "C" const char* SDLCALL SDL_GetAudioDeviceName(int, int) { std::abort(); }
extern "C" int SDLCALL SDL_GetAudioDeviceSpec(int, int, SDL_AudioSpec*) { std::abort(); }
extern "C" const char* SDLCALL SDL_GetCurrentAudioDriver() { return "test"; }
extern "C" void SDLCALL SDL_ClearQueuedAudio(SDL_AudioDeviceID) { std::abort(); }
extern "C" SDL_AudioStatus SDLCALL SDL_GetAudioDeviceStatus(SDL_AudioDeviceID) { std::abort(); }
extern "C" Uint32 SDLCALL SDL_GetQueuedAudioSize(SDL_AudioDeviceID) { std::abort(); }
extern "C" int SDLCALL SDL_QueueAudio(SDL_AudioDeviceID, const void*, Uint32) { std::abort(); }

int main() {
    AudioOut2ContextParam params{};
    sceAudioOut2ContextResetParam(&params);
    for (failAt = 0; failAt < 32; ++failAt) {
        attempts = 0;
        AudioOut2ContextHandle handle = 0x1234;
        bool threw = false;
        armed = true;
        try { sceAudioOut2ContextCreate(&params, nullptr, 0, &handle); }
        catch (const std::bad_alloc&) { threw = true; }
        armed = false;
        if (!threw) {
            if (sceAudioOut2ContextDestroy(handle) != 0) return 2;
        }
        std::size_t live = 0;
        for (auto pointer : allocations) live += pointer != nullptr;
        std::printf("allocation=%zu attempted=%zu threw=%d live=%zu handle=%llx\n", failAt, attempts, threw, live, static_cast<unsigned long long>(handle));
        if (live || (threw && handle != 0x1234)) return 1;
        if (!threw) return 0;
    }
    return 3;
}
