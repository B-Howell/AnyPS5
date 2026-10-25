#include <atomic>
#include <memory>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <vector>

#include "prx/libc/include/general/AtomicSharedPtr.hpp"

static void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template<template<class> class TAtomic>
static void CheckOwnership() {
    int address = 1;
    std::shared_ptr<int> owner(&address, [](int*) {});
    std::shared_ptr<int> other(&address, [](int*) {});
    const auto desired = std::make_shared<int>(2);
    TAtomic<std::shared_ptr<int>> slot(owner);
    auto expected = other;
    Require(!slot.compare_exchange_strong(expected, desired), "CAS accepted the same address with different ownership");
    Require(!expected.owner_before(owner) && !owner.owner_before(expected), "failed CAS did not copy the actual owner");
    Require(slot.load() == owner, "failed CAS changed the stored pointer");
    Require(slot.compare_exchange_strong(expected, desired), "CAS rejected matching ownership");
    Require(expected == owner && slot.load() == desired, "successful CAS changed expected or lost desired");

    std::shared_ptr<int> nullOwner(owner, nullptr);
    slot.store(nullOwner);
    expected.reset();
    Require(!slot.compare_exchange_strong(expected, desired), "CAS treated an owning null pointer as empty");
    Require(!expected.owner_before(owner) && !owner.owner_before(expected), "failed null CAS lost ownership");
    Require(slot.compare_exchange_strong(expected, desired), "CAS rejected matching owning null pointers");

    slot.store(nullptr);
    expected.reset();
    Require(slot.compare_exchange_strong(expected, desired), "CAS rejected two empty pointers");

    int aliasAddress = 3;
    slot.store(owner);
    expected = std::shared_ptr<int>(owner, &aliasAddress);
    Require(!slot.compare_exchange_strong(expected, desired), "CAS accepted different addresses with the same owner");

    const std::shared_ptr<int> empty;
    slot.store(std::shared_ptr<int>(empty, &address));
    expected = std::shared_ptr<int>(empty, &address);
    Require(slot.compare_exchange_strong(expected, desired), "CAS rejected matching pointers without ownership");
}

template<class T>
using TestAtomic = AtomicSharedPtr<typename T::element_type>;

template<template<class> class TAtomic>
static void CheckRelease() {
    const auto current = std::make_shared<int>(1);
    TAtomic<std::shared_ptr<int>> slot(current);
    bool released = false;
    std::shared_ptr<int> expected(new int(2), [&slot, &released](int* pointer) {
        Require(slot.load() != nullptr, "deleter could not observe the stored pointer");
        released = true;
        delete pointer;
    });
    Require(!slot.compare_exchange_strong(expected, std::make_shared<int>(3)), "mismatched CAS unexpectedly succeeded");
    Require(released && expected == current, "failed CAS did not release and replace expected");
}

template<template<class> class TAtomic>
static void CheckConcurrent() {
    TAtomic<std::shared_ptr<int>> slot(std::make_shared<int>(0));
    std::vector<std::thread> workers;
    for (int worker = 0; worker < 4; ++worker) {
        workers.emplace_back([&slot] {
            for (int iteration = 0; iteration < 1000; ++iteration) {
                auto expected = slot.load();
                while (!slot.compare_exchange_strong(expected, std::make_shared<int>(*expected + 1))) {}
            }
        });
    }
    for (auto& worker : workers) worker.join();
    Require(*slot.load() == 4000, "concurrent CAS lost an update");
}

int main(int argc, char** argv) {
    try {
        Require(argc == 2, "expected ownership, release or concurrent");
        const std::string_view test = argv[1];
        if (test == "ownership") {
            CheckOwnership<TestAtomic>();
        } else if (test == "release") {
            CheckRelease<TestAtomic>();
        } else if (test == "concurrent") {
            CheckConcurrent<TestAtomic>();
        } else {
            throw std::runtime_error("unknown atomic shared pointer case");
        }
#if defined(__cpp_lib_atomic_shared_ptr)
        std::cout << "Native atomic shared pointer ";
#else
        std::cout << "Fallback atomic shared pointer ";
#endif
        std::cout << test << " checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
