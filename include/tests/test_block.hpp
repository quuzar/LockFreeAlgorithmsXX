
#pragma once

#include <alloc/StructAllocator.hpp>

#include <algorithm>
#include <atomic>
#include <barrier>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace full_block_test_detail {

    using BF = alloc::blocks::BlockFields;
    using FLAGS = alloc::blocks::FLAGS;

    template<typename T>
    using Block = alloc::blocks::Block<T>;

    template<size_t N>
    struct Payload {
        std::byte data[N];
    };

    template<size_t N>
    struct alignas(N) AlignedPayload {
        std::byte data[N];
    };

    enum class FreeOrder {
        Forward,
        Reverse,
        Random
    };

    inline void check(bool condition, std::string_view message) {
        if (!condition)
            throw std::runtime_error(std::string(message));
    }

    template<typename F>
    void parallel(size_t workers, F&& task) {
        std::barrier start(static_cast<std::ptrdiff_t>(workers));

        std::vector<std::thread> threads;
        std::vector<std::exception_ptr> errors(workers);

        threads.reserve(workers);

        for (size_t i = 0; i < workers; ++i) {
            threads.emplace_back([&, i] {
                start.arrive_and_wait();

                try {
                    task(i);
                }
                catch (...) {
                    errors[i] = std::current_exception();
                }
            });
        }

        for (auto& thread : threads)
            thread.join();

        for (const auto& error : errors) {
            if (error)
                std::rethrow_exception(error);
        }
    }

    template<typename T>
    auto slot_index(Block<T>& block, T* ptr) -> size_t {
        const auto base = reinterpret_cast<std::uintptr_t>(
            BF::MEMORY(block)
        );

        const auto address = reinterpret_cast<std::uintptr_t>(ptr);

        const auto count = static_cast<size_t>(BF::COUNT(block));
        const auto size = static_cast<size_t>(BF::SIZE_BLOCK(block));

        check(address >= base, "Pointer below memory region");
        check(address < base + count * size, "Pointer outside memory region");
        check((address - base) % size == 0, "Invalid slot offset");
        check(address % alignof(T) == 0, "Invalid pointer alignment");

        return (address - base) / size;
    }

    template<typename T>
    void check_empty_bitmap(Block<T>& block) {
        for (size_t i = 0; i < BF::BIT_COUNT(block); ++i) {
            check(
                BF::BITMAP(block)[i].load(std::memory_order_acquire) == 0,
                "Bitmap is not empty"
            );
        }
    }

    template<typename T>
    void check_full_bitmap(Block<T>& block) {
        const auto count = static_cast<size_t>(BF::COUNT(block));
        const auto bit_count = static_cast<size_t>(BF::BIT_COUNT(block));

        for (size_t i = 0; i < bit_count; ++i) {
            const auto first = i * 64;

            const auto remaining =
                first < count ? count - first : size_t{0};

            const auto used = std::min<size_t>(remaining, 64);

            const uint64_t expected =
                used == 64
                    ? std::numeric_limits<uint64_t>::max()
                    : (uint64_t{1} << used) - 1;

            const auto actual =
                BF::BITMAP(block)[i].load(std::memory_order_acquire);

            check(actual == expected, "Incorrect full bitmap");
        }
    }

    template<typename T>
    void check_full_state(Block<T>& block) {
        const auto count = BF::COUNT(block);

        check(
            BF::OFFSET(block).load(std::memory_order_acquire) == count,
            "Incorrect offset"
        );

        check(
            BF::REGISTERED(block).load(std::memory_order_acquire) == count,
            "Incorrect registered count"
        );

        check(
            !FLAGS::INSTALLED(BF::FLAGS(block), FLAGS::FLAG_DELETE),
            "Premature FLAG_DELETE"
        );

        check_full_bitmap(block);
    }

    template<typename T>
    void check_deleted_state(Block<T>& block) {
        const auto count = BF::COUNT(block);

        check(
            BF::OFFSET(block).load(std::memory_order_acquire) == count,
            "Offset changed after deallocation"
        );

        check(
            BF::REGISTERED(block).load(std::memory_order_acquire) == count,
            "Registered count changed after deallocation"
        );

        check_empty_bitmap(block);

        check(
            FLAGS::INSTALLED(BF::FLAGS(block), FLAGS::FLAG_DELETE),
            "FLAG_DELETE is not installed"
        );

        check(
            !FLAGS::INSTALLED(BF::FLAGS(block), FLAGS::FLAG_BLOCK),
            "FLAG_BLOCK was not released"
        );
    }

    template<typename T>
    void test_layout() {
        constexpr size_t Size = std::bit_ceil(sizeof(T));

        static_assert(sizeof(T) == Size);
        static_assert(sizeof(Block<T>) == 4096);
        static_assert(alignof(Block<T>) == 4096);

        Block<T> block{};

        const auto address = reinterpret_cast<std::uintptr_t>(&block);

        const auto memory = reinterpret_cast<std::uintptr_t>(
            BF::MEMORY(block)
        );

        const auto count = static_cast<size_t>(BF::COUNT(block));
        const auto slot_size = static_cast<size_t>(BF::SIZE_BLOCK(block));

        check(address % 4096 == 0, "Block alignment failure");
        check(slot_size == Size, "Incorrect slot size");
        check(BF::SIZE_BITMAP(block) == 64, "Incorrect bitmap word size");
        check(count > 0, "Empty block");

        check(
            static_cast<size_t>(BF::BIT_COUNT(block)) * 64 >= count,
            "Insufficient bitmap capacity"
        );

        check(memory >= address, "Memory begins before block");

        check(
            memory + count * slot_size <= address + sizeof(block),
            "Memory exceeds block boundary"
        );

        check(
            memory % alignof(T) == 0,
            "Memory alignment is insufficient for T"
        );

        check(
            BF::OFFSET(block).load(std::memory_order_relaxed) == 0,
            "Initial offset is not zero"
        );

        check(
            BF::REGISTERED(block).load(std::memory_order_relaxed) == 0,
            "Initial registered count is not zero"
        );

        check(
            BF::FLAGS(block).load(std::memory_order_relaxed) == 0,
            "Initial flags are not zero"
        );

        check_empty_bitmap(block);
    }

    template<typename T>
    void test_sequential(FreeOrder order) {
        Block<T> block{};

        const auto count = static_cast<size_t>(BF::COUNT(block));
        const auto size = static_cast<size_t>(BF::SIZE_BLOCK(block));

        std::vector<T*> pointers;
        pointers.reserve(count);

        for (size_t i = 0; i < count; ++i) {
            auto* ptr = block.allocate();

            check(ptr != nullptr, "Allocation failed before capacity");
            check(slot_index(block, ptr) == i, "Incorrect allocation order");

            pointers.push_back(ptr);
        }

        check_full_state(block);

        for (size_t i = 0; i < 32; ++i) {
            check(
                block.allocate() == nullptr,
                "Allocation succeeded beyond capacity"
            );
        }

        check_full_state(block);

        for (size_t i = 0; i < count; ++i) {
            const auto value = std::byte{
                static_cast<unsigned char>((i * 37 + 19) & 0xFF)
            };

            auto* memory = BF::MEMORY(block) + i * size;

            std::fill_n(memory, sizeof(T), value);
        }

        for (size_t i = 0; i < count; ++i) {
            const auto value = std::byte{
                static_cast<unsigned char>((i * 37 + 19) & 0xFF)
            };

            auto* memory = BF::MEMORY(block) + i * size;

            check(
                std::all_of(
                    memory,
                    memory + sizeof(T),
                    [value](std::byte byte) {
                        return byte == value;
                    }
                ),
                "Slot memory corruption"
            );
        }

        std::vector<size_t> indices(count);
        std::iota(indices.begin(), indices.end(), 0);

        if (order == FreeOrder::Reverse) {
            std::reverse(indices.begin(), indices.end());
        }
        else if (order == FreeOrder::Random) {
            std::mt19937 generator(
                static_cast<uint32_t>(0xC0FFEEu + count)
            );

            std::shuffle(indices.begin(), indices.end(), generator);
        }

        size_t delete_count = 0;

        for (size_t i = 0; i < count; ++i) {
            const auto idx = indices[i];

            auto* result = block.deallocate(pointers[idx]);

            const auto bitmap_index = idx / 64;
            const auto bit_index = idx % 64;
            const auto mask = uint64_t{1} << bit_index;

            check(
                (BF::BITMAP(block)[bitmap_index].load(
                    std::memory_order_acquire
                ) & mask) == 0,
                "Deallocated slot bit is still set"
            );

            if (result != nullptr) {
                check(result == &block, "Incorrect returned block");
                ++delete_count;
            }

            if (i + 1 < count) {
                check(result == nullptr, "Premature block deletion");

                check(
                    !FLAGS::INSTALLED(
                        BF::FLAGS(block),
                        FLAGS::FLAG_DELETE
                    ),
                    "FLAG_DELETE installed before last free"
                );
            }
        }

        check(delete_count == 1, "Incorrect deletion notification count");
        check_deleted_state(block);
    }

    template<typename T>
    void test_partial_and_double_free() {
        Block<T> block{};

        const auto count = static_cast<size_t>(BF::COUNT(block));

        auto* first = block.allocate();

        check(first != nullptr, "First allocation failed");

        check(
            block.deallocate(first) == nullptr,
            "Partially used block was marked for deletion"
        );

        check_empty_bitmap(block);

        check(
            !FLAGS::INSTALLED(BF::FLAGS(block), FLAGS::FLAG_DELETE),
            "Premature deletion after partial allocation"
        );

        bool double_free_detected = false;

        try {
            block.deallocate(first);
        }
        catch (const std::runtime_error& error) {
            double_free_detected =
                std::string_view(error.what()) == "double free";
        }

        check(double_free_detected, "Double free was not detected");

        std::vector<T*> pointers;
        pointers.reserve(count - 1);

        for (size_t i = 1; i < count; ++i) {
            auto* ptr = block.allocate();

            check(ptr != nullptr, "Allocation after partial free failed");
            check(slot_index(block, ptr) == i, "Unexpected slot reuse");

            pointers.push_back(ptr);
        }

        check(
            BF::REGISTERED(block).load(std::memory_order_acquire) == count,
            "Incorrect registered count after partial free"
        );

        size_t delete_count = 0;

        for (auto* ptr : pointers) {
            auto* result = block.deallocate(ptr);

            if (result != nullptr) {
                check(result == &block, "Incorrect returned block");
                ++delete_count;
            }
        }

        check(delete_count == 1, "Block was not deleted exactly once");
        check_deleted_state(block);
    }

    template<typename T>
    void test_concurrent_double_free() {
        Block<T> block{};

        auto* ptr = block.allocate();

        check(ptr != nullptr, "Initial allocation failed");

        std::atomic<size_t> success{0};
        std::atomic<size_t> double_free{0};

        parallel(2, [&](size_t) {
            try {
                auto* result = block.deallocate(ptr);

                check(result == nullptr, "Unexpected deletion notification");

                success.fetch_add(1, std::memory_order_relaxed);
            }
            catch (const std::runtime_error& error) {
                if (std::string_view(error.what()) != "double free")
                    throw;

                double_free.fetch_add(1, std::memory_order_relaxed);
            }
        });

        check(success.load() == 1, "Incorrect successful free count");
        check(double_free.load() == 1, "Concurrent double free not detected");

        check_empty_bitmap(block);

        check(
            !FLAGS::INSTALLED(BF::FLAGS(block), FLAGS::FLAG_BLOCK),
            "Block remained locked"
        );
    }

    template<typename T>
    void test_parallel_full(size_t workers) {
        Block<T> block{};

        const auto count = static_cast<size_t>(BF::COUNT(block));

        std::vector<std::vector<T*>> owned(workers);
        std::vector<std::atomic<uint8_t>> seen(count);

        for (auto& item : seen)
            item.store(0, std::memory_order_relaxed);

        parallel(workers, [&](size_t thread_index) {
            auto& local = owned[thread_index];

            while (auto* ptr = block.allocate()) {
                const auto idx = slot_index(block, ptr);

                check(
                    seen[idx].fetch_add(
                        1, std::memory_order_relaxed
                    ) == 0,
                    "Duplicate slot allocation"
                );

                local.push_back(ptr);
            }
        });

        size_t total = 0;

        for (const auto& local : owned)
            total += local.size();

        check(total == count, "Incorrect total allocation count");

        for (const auto& item : seen) {
            check(
                item.load(std::memory_order_relaxed) == 1,
                "Slot was not allocated exactly once"
            );
        }

        check_full_state(block);

        std::atomic<size_t> delete_count{0};

        parallel(workers, [&](size_t thread_index) {
            for (auto* ptr : owned[thread_index]) {
                auto* result = block.deallocate(ptr);

                if (result != nullptr) {
                    check(result == &block, "Incorrect returned block");

                    delete_count.fetch_add(
                        1, std::memory_order_relaxed
                    );
                }
            }
        });

        check(
            delete_count.load(std::memory_order_relaxed) == 1,
            "Concurrent deletion notification count is not one"
        );

        check_deleted_state(block);
    }

    template<typename T>
    void test_parallel_interleaved(size_t workers) {
        Block<T> block{};

        const auto count = static_cast<size_t>(BF::COUNT(block));

        std::vector<std::atomic<uint8_t>> seen(count);

        for (auto& item : seen)
            item.store(0, std::memory_order_relaxed);

        std::atomic<size_t> allocated{0};
        std::atomic<size_t> freed{0};
        std::atomic<size_t> delete_count{0};

        parallel(workers, [&](size_t thread_index) {
            while (auto* ptr = block.allocate()) {
                const auto idx = slot_index(block, ptr);

                check(
                    seen[idx].fetch_add(
                        1, std::memory_order_relaxed
                    ) == 0,
                    "Duplicate allocation during interleaving"
                );

                allocated.fetch_add(1, std::memory_order_relaxed);

                if ((idx + thread_index) % 3 == 0)
                    std::this_thread::yield();

                auto* result = block.deallocate(ptr);

                freed.fetch_add(1, std::memory_order_relaxed);

                if (result != nullptr) {
                    check(result == &block, "Incorrect returned block");

                    delete_count.fetch_add(
                        1, std::memory_order_relaxed
                    );
                }
            }
        });

        check(
            allocated.load(std::memory_order_relaxed) == count,
            "Incorrect interleaved allocation count"
        );

        check(
            freed.load(std::memory_order_relaxed) == count,
            "Incorrect interleaved deallocation count"
        );

        for (const auto& item : seen) {
            check(
                item.load(std::memory_order_relaxed) == 1,
                "Interleaved slot was not allocated exactly once"
            );
        }

        check(
            delete_count.load(std::memory_order_relaxed) == 1,
            "Incorrect interleaved deletion notification count"
        );

        check_deleted_state(block);
    }

    template<typename T>
    void run_type(
        std::string_view name,
        size_t rounds,
        size_t workers
    ) {
        auto stage = [&](std::string_view stage_name, auto&& task) {
            try {
                task();
            }
            catch (const std::exception& error) {
                throw std::runtime_error(
                    std::string(name) + " / " +
                    std::string(stage_name) + ": " +
                    error.what()
                );
            }
        };

        stage("layout", [] {
            test_layout<T>();
        });

        stage("sequential forward", [] {
            test_sequential<T>(FreeOrder::Forward);
        });

        stage("sequential reverse", [] {
            test_sequential<T>(FreeOrder::Reverse);
        });

        stage("sequential random", [] {
            test_sequential<T>(FreeOrder::Random);
        });

        stage("partial and double free", [] {
            test_partial_and_double_free<T>();
        });

        stage("concurrent double free", [] {
            test_concurrent_double_free<T>();
        });

        for (size_t round = 0; round < rounds; ++round) {
            const auto round_name = std::to_string(round + 1);

            stage("parallel full round " + round_name, [&] {
                test_parallel_full<T>(workers);
            });

            stage("interleaved round " + round_name, [&] {
                test_parallel_interleaved<T>(workers);
            });
        }

        std::cout
            << "Block " << name
            << ": PASS (" << rounds << " stress rounds)"
            << '\n';
    }

    template<size_t Size>
    void run_size(size_t rounds, size_t workers) {
        run_type<Payload<Size>>(
            std::to_string(Size) + "B / plain",
            rounds,
            workers
        );

        run_type<AlignedPayload<Size>>(
            std::to_string(Size) + "B / aligned",
            rounds,
            workers
        );
    }
}

inline void test_full_block(
    size_t rounds = 32,
    size_t workers = 8
) {
    using namespace full_block_test_detail;

    if (rounds == 0)
        throw std::invalid_argument("Rounds must be greater than zero");

    if (workers < 2)
        throw std::invalid_argument("At least two workers are required");

    run_size<1>(rounds, workers);
    run_size<2>(rounds, workers);
    run_size<4>(rounds, workers);
    run_size<8>(rounds, workers);
    run_size<16>(rounds, workers);
    run_size<32>(rounds, workers);
    run_size<64>(rounds, workers);
    run_size<128>(rounds, workers);
    run_size<256>(rounds, workers);

    std::cout
        << "\nFull Block Test: PASS"
        << "\nSpecializations: 9"
        << "\nType variants: 18"
        << "\nStress rounds per variant: " << rounds
        << "\nWorker threads: " << workers
        << '\n';
}
