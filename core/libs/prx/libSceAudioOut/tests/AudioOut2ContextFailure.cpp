#define SDL_MAIN_HANDLED
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include "prx/libSceAudioOut/include/AudioOut2.hpp"
#include "SDL.h"

static bool armed = false;
static bool withDevice = false;
static std::size_t liveDevices = 0;
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
int InitializeSdlAudio_nid_no_patch() { return withDevice ? 0 : -1; }
extern "C" const char* SDLCALL SDL_GetError() { return "test: no audio device"; }
extern "C" SDL_AudioDeviceID SDLCALL SDL_OpenAudioDevice(const char*, int capture, const SDL_AudioSpec* desired, SDL_AudioSpec* obtained, int changes) {
    if (!withDevice || capture != 0 || !desired || !obtained || changes != 0 || liveDevices != 0) std::abort();
    *obtained = *desired;
    ++liveDevices;
    return 1;
}
extern "C" void SDLCALL SDL_PauseAudioDevice(SDL_AudioDeviceID device, int pause) {
    if (!withDevice || device != 1 || liveDevices != 1 || pause != 0) std::abort();
}
extern "C" Uint32 SDLCALL SDL_WasInit(Uint32 flags) {
    if (withDevice && flags != SDL_INIT_AUDIO) std::abort();
    return withDevice ? SDL_INIT_AUDIO : 0;
}
extern "C" void SDLCALL SDL_CloseAudioDevice(SDL_AudioDeviceID device) {
    if (!withDevice || device != 1 || liveDevices != 1) std::abort();
    --liveDevices;
}
extern "C" int SDLCALL SDL_GetNumAudioDevices(int capture) {
    if (withDevice && capture != 0) std::abort();
    return withDevice ? 1 : 0;
}
extern "C" const char* SDLCALL SDL_GetAudioDeviceName(int, int) { std::abort(); }
extern "C" int SDLCALL SDL_GetAudioDeviceSpec(int, int, SDL_AudioSpec*) { std::abort(); }
extern "C" const char* SDLCALL SDL_GetCurrentAudioDriver() { return "test"; }
extern "C" void SDLCALL SDL_ClearQueuedAudio(SDL_AudioDeviceID) { std::abort(); }
extern "C" SDL_AudioStatus SDLCALL SDL_GetAudioDeviceStatus(SDL_AudioDeviceID) { std::abort(); }
extern "C" Uint32 SDLCALL SDL_GetQueuedAudioSize(SDL_AudioDeviceID) { std::abort(); }
extern "C" int SDLCALL SDL_QueueAudio(SDL_AudioDeviceID, const void*, Uint32) { std::abort(); }

int main(int argc, char** argv) {
    if (argc == 2 && std::strcmp(argv[1], "--device") == 0) withDevice = true;
    else if (argc != 1) return 5;
    try { throw std::bad_alloc{}; }
    catch (const std::bad_alloc&) {}
    AudioOut2ContextParam params{};
    if (sceAudioOut2ContextResetParam(&params) != 0) return 4;
    AudioOut2ContextHandle warmup = 0;
    if (sceAudioOut2ContextCreate(&params, nullptr, 0, &warmup) != 0 || warmup == 0) return 4;
    if (sceAudioOut2ContextDestroy(warmup) != 0 || liveDevices != 0) return 4;
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
        std::printf("allocation=%zu attempted=%zu threw=%d live=%zu handle=%llx devices=%zu\n", failAt, attempts, threw, live, static_cast<unsigned long long>(handle), liveDevices);
        if (live || liveDevices || (threw && handle != 0x1234)) return 1;
        if (!threw) return 0;
    }
    return 3;
}
