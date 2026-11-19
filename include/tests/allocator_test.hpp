
#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <barrier>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <unordered_set>
#include <vector>

template<template<typename> class Allocator>
void test_allocator() {
    struct alignas(256) Value {
        uint64_t id = 0;
        uint64_t checksum = 0;
        std::array<uint64_t, 6> data{};
    };

    struct alignas(256) LargeValue {
        std::array<uint64_t, 512> data{};
    };

    constexpr uint64_t checksum_key = 0x9E3779B97F4A7C15ULL;

    auto check = [](bool condition, const char* message) {
        if (!condition) {
            throw std::runtime_error(message);
        }
    };

    auto start_time = std::chrono::steady_clock::now();

    std::cout << "\n========== ALLOCATOR TEST ==========\n";

    {
        Allocator<Value> allocator;

        constexpr size_t count = 10000;

        for (size_t i = 0; i < count; ++i) {
            auto* ptr = allocator.allocate();

            check(ptr != nullptr, "Single: null pointer");
            check(
                reinterpret_cast<uintptr_t>(ptr) % alignof(Value) == 0,
                "Single: invalid alignment"
            );

            std::construct_at(ptr);

            ptr->id = i;
            ptr->checksum = i ^ checksum_key;
            ptr->data.fill(i);

            check(ptr->id == i, "Single: corrupted id");
            check(
                ptr->checksum == (i ^ checksum_key),
                "Single: corrupted checksum"
            );
            check(ptr->data[0] == i, "Single: corrupted data");

            std::destroy_at(ptr);
            allocator.deallocate(ptr);
        }

        std::cout << "[1/9] Single allocation: PASS (" << count << ")\n";
    }

    {
        Allocator<Value> allocator;

        constexpr std::array<size_t, 8> sizes{
            1, 31, 32, 33, 63, 64, 65, 1024
        };

        for (auto count : sizes) {
            std::vector<Value*> pointers;
            std::unordered_set<Value*> addresses;

            pointers.reserve(count);
            addresses.reserve(count);

            for (size_t i = 0; i < count; ++i) {
                auto* ptr = allocator.allocate();

                check(ptr != nullptr, "Boundary: null pointer");
                check(
                    reinterpret_cast<uintptr_t>(ptr) % alignof(Value) == 0,
                    "Boundary: invalid alignment"
                );
                check(
                    addresses.insert(ptr).second,
                    "Boundary: duplicate active pointer"
                );

                std::construct_at(ptr);

                ptr->id = i;
                ptr->checksum = i ^ checksum_key;
                ptr->data.fill(i);

                pointers.push_back(ptr);
            }

            for (size_t i = 0; i < count; ++i) {
                auto* ptr = pointers[i];

                check(ptr->id == i, "Boundary: corrupted id");
                check(
                    ptr->checksum == (i ^ checksum_key),
                    "Boundary: corrupted checksum"
                );
                check(ptr->data[5] == i, "Boundary: corrupted data");
            }

            for (auto* ptr : pointers) {
                std::destroy_at(ptr);
                allocator.deallocate(ptr);
            }
        }

        std::cout << "[2/9] Block boundaries: PASS\n";
    }

    {
        Allocator<Value> allocator;

        constexpr size_t count = 131072;

        std::vector<Value*> pointers;
        std::unordered_set<Value*> addresses;

        pointers.reserve(count);
        addresses.reserve(count);

        for (size_t i = 0; i < count; ++i) {
            auto* ptr = allocator.allocate();

            check(ptr != nullptr, "Mass: null pointer");
            check(
                reinterpret_cast<uintptr_t>(ptr) % alignof(Value) == 0,
                "Mass: invalid alignment"
            );
            check(
                addresses.insert(ptr).second,
                "Mass: duplicate active pointer"
            );

            std::construct_at(ptr);

            ptr->id = i;
            ptr->checksum = i ^ checksum_key;
            ptr->data.fill(i);

            pointers.push_back(ptr);
        }

        for (size_t i = 0; i < count; ++i) {
            auto* ptr = pointers[i];

            check(ptr->id == i, "Mass: corrupted id");
            check(
                ptr->checksum == (i ^ checksum_key),
                "Mass: corrupted checksum"
            );
            check(ptr->data[0] == i, "Mass: corrupted data");
            check(ptr->data[5] == i, "Mass: corrupted data");
        }

        for (size_t i = count; i > 0; --i) {
            auto* ptr = pointers[i - 1];

            std::destroy_at(ptr);
            allocator.deallocate(ptr);
        }

        std::cout << "[3/9] Mass allocation: PASS (" << count << ")\n";
    }

    {
        Allocator<LargeValue> allocator;

        constexpr size_t count = 2048;

        std::vector<LargeValue*> pointers;
        std::unordered_set<LargeValue*> addresses;

        pointers.reserve(count);
        addresses.reserve(count);

        for (size_t i = 0; i < count; ++i) {
            auto* ptr = allocator.allocate();

            check(ptr != nullptr, "Large object: null pointer");
            check(
                reinterpret_cast<uintptr_t>(ptr) % alignof(LargeValue) == 0,
                "Large object: invalid alignment"
            );
            check(
                addresses.insert(ptr).second,
                "Large object: duplicate active pointer"
            );

            std::construct_at(ptr);
            ptr->data.fill(i ^ checksum_key);

            pointers.push_back(ptr);
        }

        for (size_t i = 0; i < count; ++i) {
            for (auto value : pointers[i]->data) {
                check(
                    value == (i ^ checksum_key),
                    "Large object: corrupted data"
                );
            }
        }

        for (auto* ptr : pointers) {
            std::destroy_at(ptr);
            allocator.deallocate(ptr);
        }

        std::cout << "[4/9] Large objects: PASS ("
                  << count << " x " << sizeof(LargeValue)
                  << " bytes)\n";
    }

    {
        Allocator<Value> allocator;

        constexpr size_t count = 4096;

        std::vector<Value*> pointers;
        std::unordered_set<uintptr_t> previous_addresses;

        pointers.reserve(count);
        previous_addresses.reserve(count);

        for (size_t i = 0; i < count; ++i) {
            auto* ptr = allocator.allocate();

            check(ptr != nullptr, "Reuse: null pointer");

            previous_addresses.insert(
                reinterpret_cast<uintptr_t>(ptr)
            );

            std::construct_at(ptr);
            ptr->id = i;
            pointers.push_back(ptr);
        }

        for (auto* ptr : pointers) {
            std::destroy_at(ptr);
            allocator.deallocate(ptr);
        }

        pointers.clear();

        size_t reused = 0;
        std::unordered_set<Value*> active;

        for (size_t i = 0; i < count; ++i) {
            auto* ptr = allocator.allocate();

            check(ptr != nullptr, "Reuse: null pointer");
            check(
                active.insert(ptr).second,
                "Reuse: duplicate active pointer"
            );

            if (previous_addresses.contains(
                reinterpret_cast<uintptr_t>(ptr)
            )) {
                ++reused;
            }

            std::construct_at(ptr);
            ptr->id = i;
            pointers.push_back(ptr);
        }

        check(reused > 0, "Reuse: no addresses reused");

        for (auto* ptr : pointers) {
            std::destroy_at(ptr);
            allocator.deallocate(ptr);
        }

        std::cout << "[5/9] Reuse: PASS ("
                  << reused << "/" << count
                  << " addresses reused)\n";
    }

    {
        Allocator<Value> allocator;

        constexpr size_t count = 8192;
        constexpr size_t thread_count = 8;

        std::vector<Value*> pointers;
        std::unordered_set<uintptr_t> previous_addresses;

        pointers.reserve(count);
        previous_addresses.reserve(count);

        for (size_t i = 0; i < count; ++i) {
            auto* ptr = allocator.allocate();

            check(ptr != nullptr, "Cross-thread: null pointer");

            std::construct_at(ptr);
            ptr->id = i;
            ptr->checksum = i ^ checksum_key;

            pointers.push_back(ptr);
            previous_addresses.insert(
                reinterpret_cast<uintptr_t>(ptr)
            );
        }

        std::atomic<bool> valid{true};
        std::vector<std::jthread> threads;

        threads.reserve(thread_count);

        for (size_t t = 0; t < thread_count; ++t) {
            threads.emplace_back([&, t]() {
                for (size_t i = t; i < count; i += thread_count) {
                    auto* ptr = pointers[i];

                    if (ptr->id != i ||
                        ptr->checksum != (i ^ checksum_key)) {
                        valid.store(false, std::memory_order_relaxed);
                    }

                    std::destroy_at(ptr);
                    allocator.deallocate(ptr);
                }
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }

        check(
            valid.load(std::memory_order_relaxed),
            "Cross-thread: corrupted data"
        );

        size_t reused = 0;
        std::unordered_set<Value*> active;


        for (size_t i = 0; i < count; ++i) {
            auto* ptr = allocator.allocate();

            check(ptr != nullptr, "Cross-thread: reuse failed");
            check(
                active.insert(ptr).second,
                "Cross-thread: duplicate active pointer"
            );

            if (previous_addresses.contains(
                reinterpret_cast<uintptr_t>(ptr)
            )) {
                ++reused;
            }

            std::construct_at(ptr);
            std::destroy_at(ptr);

            check(
                active.erase(ptr) == 1,
                "Cross-thread: missing active pointer"
            );

            allocator.deallocate(ptr);
        }


        check(reused > 0, "Cross-thread: no reuse");

        std::cout << "[6/9] Cross-thread deallocation: PASS ("
                  << count << " objects)\n";
    }

    {
        Allocator<Value> allocator;

        constexpr size_t thread_count = 8;
        constexpr size_t operations = 30000;
        constexpr size_t max_active = 64;

        std::barrier start(thread_count);

        std::unordered_set<Value*> active;
        std::mutex active_mutex;

        std::exception_ptr error;
        std::mutex error_mutex;

        std::vector<std::jthread> threads;
        threads.reserve(thread_count);

        for (size_t t = 0; t < thread_count; ++t) {
            threads.emplace_back([&, t]() {
                try {
                    start.arrive_and_wait();

                    std::vector<Value*> local;
                    local.reserve(max_active);

                    uint64_t state = checksum_key ^ (t + 1);

                    for (size_t i = 0; i < operations; ++i) {
                        state ^= state << 13;
                        state ^= state >> 7;
                        state ^= state << 17;

                        bool should_allocate =
                            local.empty() ||
                            (local.size() < max_active &&
                             (state & 3) != 0);

                        if (should_allocate) {
                            auto* ptr = allocator.allocate();

                            check(ptr != nullptr, "Concurrent: null pointer");
                            check(
                                reinterpret_cast<uintptr_t>(ptr) %
                                    alignof(Value) == 0,
                                "Concurrent: invalid alignment"
                            );

                            {
                                std::lock_guard guard(active_mutex);

                                check(
                                    active.insert(ptr).second,
                                    "Concurrent: duplicate active pointer"
                                );
                            }

                            std::construct_at(ptr);

                            auto id = (uint64_t(t) << 48) | (i + 1);

                            ptr->id = id;
                            ptr->checksum = id ^ checksum_key;
                            ptr->data.fill(id);

                            local.push_back(ptr);
                        } else {
                            auto idx = state % local.size();
                            auto* ptr = local[idx];

                            check(
                                ptr->checksum == (ptr->id ^ checksum_key),
                                "Concurrent: corrupted checksum"
                            );
                            check(
                                ptr->data[0] == ptr->id &&
                                ptr->data[5] == ptr->id,
                                "Concurrent: corrupted data"
                            );

                            {
                                std::lock_guard guard(active_mutex);

                                std::destroy_at(ptr);
                                allocator.deallocate(ptr);

                                check(
                                    active.erase(ptr) == 1,
                                    "Concurrent: missing active pointer"
                                );
                            }

                            local[idx] = local.back();
                            local.pop_back();
                        }
                    }

                    for (auto* ptr : local) {
                        check(
                            ptr->checksum == (ptr->id ^ checksum_key),
                            "Concurrent: final checksum mismatch"
                        );

                        {
                            std::lock_guard guard(active_mutex);

                            std::destroy_at(ptr);
                            allocator.deallocate(ptr);

                            check(
                                active.erase(ptr) == 1,
                                "Concurrent: final pointer mismatch"
                            );
                        }
                    }
                } catch (...) {
                    std::lock_guard guard(error_mutex);

                    if (!error) {
                        error = std::current_exception();
                    }
                }
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }

        if (error) {
            std::rethrow_exception(error);
        }

        check(active.empty(), "Concurrent: leaked active pointers");

        std::cout << "[7/9] Concurrent mixed operations: PASS ("
                  << thread_count << " threads, "
                  << thread_count * operations << " operations)\n";
    }

    {
        Allocator<Value> allocator;

        constexpr size_t thread_count = 8;
        constexpr size_t rounds = 48;
        constexpr size_t batch_size = 256;

        std::barrier start(thread_count);

        std::exception_ptr error;
        std::mutex error_mutex;

        std::vector<std::jthread> threads;
        threads.reserve(thread_count);

        for (size_t t = 0; t < thread_count; ++t) {
            threads.emplace_back([&, t]() {
                try {
                    start.arrive_and_wait();

                    std::vector<Value*> pointers;
                    pointers.reserve(batch_size);

                    for (size_t round = 0; round < rounds; ++round) {
                        pointers.clear();

                        for (size_t i = 0; i < batch_size; ++i) {
                            auto* ptr = allocator.allocate();

                            check(
                                ptr != nullptr,
                                "Stress: null pointer"
                            );

                            std::construct_at(ptr);

                            auto id =
                                (uint64_t(t) << 48) |
                                (uint64_t(round) << 16) |
                                i;

                            ptr->id = id;
                            ptr->checksum = id ^ checksum_key;
                            ptr->data.fill(id);

                            pointers.push_back(ptr);
                        }

                        for (size_t i = batch_size; i > 0; --i) {
                            auto* ptr = pointers[i - 1];

                            check(
                                ptr->checksum == (ptr->id ^ checksum_key),
                                "Stress: corrupted checksum"
                            );
                            check(
                                ptr->data[0] == ptr->id &&
                                ptr->data[5] == ptr->id,
                                "Stress: corrupted data"
                            );

                            std::destroy_at(ptr);
                            allocator.deallocate(ptr);
                        }
                    }
                } catch (...) {
                    std::lock_guard guard(error_mutex);

                    if (!error) {
                        error = std::current_exception();
                    }
                }
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }

        if (error) {
            std::rethrow_exception(error);
        }

        std::cout << "[8/9] High-contention stress: PASS ("
                  << thread_count << " threads, "
                  << thread_count * rounds * batch_size
                  << " allocations)\n";
    }

    {
        Allocator<Value> allocator;

        if constexpr (requires(Allocator<Value>& a) { a.reset(); }) {
            std::vector<Value*> pointers;

            for (size_t i = 0; i < 1024; ++i) {
                auto* ptr = allocator.allocate();

                check(ptr != nullptr, "Reset: null pointer");

                std::construct_at(ptr);
                ptr->id = i;

                pointers.push_back(ptr);
            }

            for (auto* ptr : pointers) {
                std::destroy_at(ptr);
                allocator.deallocate(ptr);
            }

            allocator.reset();

            pointers.clear();

            for (size_t i = 0; i < 1024; ++i) {
                auto* ptr = allocator.allocate();

                check(ptr != nullptr, "Reset: allocation failed");

                std::construct_at(ptr);

                ptr->id = i;
                ptr->checksum = i ^ checksum_key;

                pointers.push_back(ptr);
            }

            for (size_t i = 0; i < pointers.size(); ++i) {
                check(
                    pointers[i]->id == i,
                    "Reset: corrupted id"
                );
                check(
                    pointers[i]->checksum == (i ^ checksum_key),
                    "Reset: corrupted checksum"
                );

                std::destroy_at(pointers[i]);
                allocator.deallocate(pointers[i]);
            }

            std::cout << "[9/9] Reset: PASS\n";
        } else {
            std::cout << "[9/9] Reset: SKIPPED\n";
        }
    }

    auto elapsed = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start_time
    ).count();

    std::cout << "====================================\n";
    std::cout << "ALL TESTS PASSED\n";
    std::cout << "Elapsed: " << elapsed << " ms\n";
    std::cout << "====================================\n";
}
