#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>

class FileStream;

extern "C" {
int APS5_VABI sceKernelOpen(const char*, int, std::uint16_t);
std::int64_t APS5_VABI sceKernelRead(int, void*, std::size_t);
std::int64_t APS5_VABI sceKernelWrite(int, const void*, std::size_t);
std::int64_t APS5_VABI sceKernelPwrite(int, const void*, std::size_t, std::int64_t);
std::int64_t APS5_VABI sceKernelLseek(int, std::int64_t, int);
int APS5_VABI fcntl_nid_postfix(int, int, ...);
FileStream* APS5_VABI fopen_nid_postfix(const char*, const char*);
FileStream* APS5_VABI fdopen_nid_postfix(int, const char*);
FileStream* APS5_VABI freopen_nid_postfix(const char*, const char*, FileStream*);
int APS5_VABI fclose_nid_postfix(FileStream*);
int APS5_VABI fileno_nid_postfix(FileStream*);
}

static void Check(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "stream close check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static constexpr const char* Appended = "guest_stream_close_appended.txt";
static constexpr const char* Reused = "guest_stream_close_reused.txt";
static constexpr int ReadWrite = 0x0002;
static constexpr int Append = 0x0008;
static constexpr int CreateWriteAppend = 0x0209;
static constexpr int Truncate = 0x0400;
static constexpr int CloseOnExec = 0x00100000;
static constexpr int GetDescriptorFlags = 1;
static constexpr int GetStatusFlags = 3;
static constexpr int FromStart = 0;

static void Store(const char* path, const std::string& contents) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << contents;
}

static std::string Contents(const char* path) {
    std::ifstream file(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

static FileStream* Reopen(int closed, const char* mode) {
    auto* stream = fopen_nid_postfix(Reused, mode);
    Require(stream != nullptr);
    Require(fileno_nid_postfix(stream) == closed);
    return stream;
}

int main() {
    Store(Reused, "0123456789");
    const int appending = sceKernelOpen(Appended, CreateWriteAppend | Truncate | CloseOnExec, 0644);
    Require(appending >= 0);
    Require((fcntl_nid_postfix(appending, GetStatusFlags) & Append) != 0);
    Require(fcntl_nid_postfix(appending, GetDescriptorFlags) == 1);
    auto* stream = fdopen_nid_postfix(appending, "a");
    Require(stream != nullptr);
    Require(fclose_nid_postfix(stream) == 0);

    stream = Reopen(appending, "r+");
    Require(fcntl_nid_postfix(appending, GetStatusFlags) == ReadWrite);
    Require(fcntl_nid_postfix(appending, GetDescriptorFlags) == 0);
    Require(sceKernelPwrite(appending, "AB", 2, 2) == 2);
    Require(sceKernelLseek(appending, 4, FromStart) == 4);
    Require(sceKernelWrite(appending, "CD", 2) == 2);
    Require(fclose_nid_postfix(stream) == 0);
    Require(Contents(Reused) == "01ABCD6789");

    const int random = sceKernelOpen("/dev/urandom", 0, 0);
    Require(random >= 0);
    stream = fdopen_nid_postfix(random, "r");
    Require(stream != nullptr);
    Require(fclose_nid_postfix(stream) == 0);

    stream = Reopen(random, "r");
    char read[16]{};
    Require(sceKernelRead(random, read, sizeof(read)) == 10);
    Require(std::string(read, 10) == "01ABCD6789");
    Require(fclose_nid_postfix(stream) == 0);

    const int replaced = sceKernelOpen(Appended, CreateWriteAppend | CloseOnExec, 0644);
    Require(replaced >= 0);
    stream = fdopen_nid_postfix(replaced, "a");
    Require(stream != nullptr);
    stream = freopen_nid_postfix(Reused, "r+", stream);
    Require(stream != nullptr);
    Require(fileno_nid_postfix(stream) == replaced);
    Require(fcntl_nid_postfix(replaced, GetStatusFlags) == ReadWrite);
    Require(fcntl_nid_postfix(replaced, GetDescriptorFlags) == 0);
    Require(sceKernelPwrite(replaced, "EF", 2, 8) == 2);
    Require(fclose_nid_postfix(stream) == 0);
    Require(Contents(Reused) == "01ABCD67EF");

    std::remove(Appended);
    std::remove(Reused);
}
