
#pragma once

#include <alloc/StructAllocator.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <barrier>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <vector>

namespace slotblock_tests {
    using namespace alloc;
    using namespace alloc::blocks;

    template<std::size_t N>
    struct Payload {
        std::uint8_t data[N];
    };

    template<std::size_t N>
    struct alignas(N) AlignedPayload {
        std::uint8_t data[N];
    };

    inline void require(
        bool condition,
        std::string_view label,
        std::string_view message
    ) {
        if (!condition) {
            throw std::runtime_error(
                std::string(label) + ": " + std::string(message)
            );
        }
    }

    template<typename T>
    auto slot_index(
        const SlotBlock<T>& block,
        T* ptr
    ) -> std::size_t {
        const auto address = reinterpret_cast<std::uintptr_t>(ptr);
        const auto base = reinterpret_cast<std::uintptr_t>(&block.memory[0]);

        if (address < base) {
            return SlotBlock<T>::count;
        }

        const auto distance = address - base;

        if (distance % sizeof(Slot<T>) != 0) {
            return SlotBlock<T>::count;
        }

        const auto idx = distance / sizeof(Slot<T>);

        return idx < SlotBlock<T>::count
            ? idx
            : SlotBlock<T>::count;
    }

    template<typename T>
    auto valid_slot(
        SlotBlock<T>& block,
        T* ptr
    ) -> bool {
        if (!ptr) {
            return false;
        }

        const auto idx = slot_index(block, ptr);

        if (idx >= SlotBlock<T>::count) {
            return false;
        }

        if (reinterpret_cast<std::uintptr_t>(ptr) % alignof(T) != 0) {
            return false;
        }

        if (ptr != reinterpret_cast<T*>(block.memory[idx].memory)) {
            return false;
        }

        const auto* slot = reinterpret_cast<const Slot<T>*>(ptr);

        return slot == &block.memory[idx] && slot->owner == &block;
    }

    template<typename T>
    void check_initial(
        SlotBlock<T>& block,
        std::string_view label
    ) {
        require(block.offset.load() == 0, label, "initial offset");
        require(block.registered.load() == 0, label, "initial registered");
        require(block.flags.load() == 0, label, "initial flags");
        require(block.cur_bit.load() == 0, label, "initial cur_bit");
        require(block.cur_map.load() == 0, label, "initial cur_map");
        require(block.bitmap[0].load() == 0, label, "initial bitmap");

        for (const auto& slot : block.memory) {
            require(slot.owner == &block, label, "initial owner");
        }
    }

    inline auto pattern(
        std::size_t slot,
        std::size_t byte,
        std::size_t salt
    ) -> std::uint8_t {
        return static_cast<std::uint8_t>(
            (slot * 73 + byte * 29 + salt * 11) & 255
        );
    }

    template<typename T>
    void write_pattern(
        T* ptr,
        std::size_t slot,
        std::size_t salt
    ) {
        auto* bytes = reinterpret_cast<std::uint8_t*>(ptr);

        for (std::size_t i = 0; i < sizeof(T); ++i) {
            bytes[i] = pattern(slot, i, salt);
        }
    }

    template<typename T>
    auto verify_pattern(
        T* ptr,
        std::size_t slot,
        std::size_t salt
    ) -> bool {
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(ptr);

        for (std::size_t i = 0; i < sizeof(T); ++i) {
            if (bytes[i] != pattern(slot, i, salt)) {
                return false;
            }
        }

        return true;
    }

    template<typename T>
    void test_sequential(std::string_view label) {
        constexpr auto count = SlotBlock<T>::count;

        static_assert(count == 32);
        static_assert(SlotBlock<T>::bit_count == 1);
        static_assert(std::is_standard_layout_v<Slot<T>>);
        static_assert(offsetof(Slot<T>, memory) == 0);
        static_assert(alignof(Slot<T>) >= alignof(T));
        static_assert(sizeof(Slot<T>) % alignof(T) == 0);

        for (std::size_t order = 0; order < 3; ++order) {
            auto block = std::make_unique<SlotBlock<T>>();
            check_initial(*block, label);

            std::array<T*, count> pointers{};

            for (std::size_t i = 0; i < count; ++i) {
                auto* ptr = block->allocate();

                require(
                    valid_slot(*block, ptr),
                    label,
                    "sequential slot or owner"
                );

                require(
                    slot_index(*block, ptr) == i,
                    label,
                    "sequential index"
                );

                pointers[i] = ptr;
                write_pattern(ptr, i, order);

                const auto expected = static_cast<std::uint32_t>(
                    (std::uint64_t{1} << (i + 1)) - 1
                );

                require(
                    block->bitmap[0].load() == expected,
                    label,
                    "sequential bitmap"
                );

                require(
                    block->offset.load() == i + 1,
                    label,
                    "sequential offset"
                );

                require(
                    block->registered.load() == i + 1,
                    label,
                    "sequential registered"
                );

                require(
                    !FLAGS::INSTALLED(block->flags, FLAGS::FLAG_DELETE),
                    label,
                    "premature deletion"
                );
            }

            for (std::size_t i = 0; i < 64; ++i) {
                require(
                    block->allocate() == nullptr,
                    label,
                    "overflow must return nullptr"
                );
            }

            require(
                block->offset.load() == count,
                label,
                "offset after overflow"
            );

            require(
                block->registered.load() == count,
                label,
                "registered after overflow"
            );

            require(
                block->bitmap[0].load() == UINT32_MAX,
                label,
                "bitmap after overflow"
            );

            for (std::size_t i = 0; i < count; ++i) {
                require(
                    verify_pattern(pointers[i], i, order),
                    label,
                    "data corruption before free"
                );

                require(
                    valid_slot(*block, pointers[i]),
                    label,
                    "owner changed before free"
                );
            }

            std::array<std::size_t, count> indices{};
            std::iota(indices.begin(), indices.end(), 0);

            if (order == 1) {
                std::reverse(indices.begin(), indices.end());
            } else if (order == 2) {
                std::mt19937 rng(0x5A17u);
                std::shuffle(indices.begin(), indices.end(), rng);
            }

            std::uint32_t expected_bitmap = UINT32_MAX;

            for (std::size_t i = 0; i < count; ++i) {
                const auto idx = indices[i];

                require(
                    verify_pattern(pointers[idx], idx, order),
                    label,
                    "data corruption during free"
                );

                auto* retired = block->deallocate(pointers[idx]);

                expected_bitmap &= ~(std::uint32_t{1} << idx);

                require(
                    block->bitmap[0].load() == expected_bitmap,
                    label,
                    "bitmap after free"
                );

                require(
                    retired == (i + 1 == count ? block.get() : nullptr),
                    label,
                    "retirement notification"
                );

                require(
                    FLAGS::INSTALLED(block->flags, FLAGS::FLAG_DELETE)
                        == (i + 1 == count),
                    label,
                    "delete flag"
                );

                require(
                    block->registered.load() == count,
                    label,
                    "registered changed on free"
                );
            }

            bool threw = false;

            try {
                block->deallocate(pointers[indices.back()]);
            } catch (const std::runtime_error&) {
                threw = true;
            }

            require(
                threw,
                label,
                "double free after retirement not detected"
            );

            require(
                block->bitmap[0].load() == 0,
                label,
                "bitmap changed after double free"
            );
        }
    }

    template<typename T>
    void test_partial(std::string_view label) {
        constexpr auto count = SlotBlock<T>::count;

        auto block = std::make_unique<SlotBlock<T>>();
        check_initial(*block, label);

        std::array<T*, count> pointers{};
        constexpr std::size_t first = 11;

        for (std::size_t i = 0; i < first; ++i) {
            pointers[i] = block->allocate();

            require(
                valid_slot(*block, pointers[i]),
                label,
                "partial slot or owner"
            );
        }

        for (std::size_t i = 0; i < first; ++i) {
            require(
                block->deallocate(pointers[i]) == nullptr,
                label,
                "early retirement"
            );
        }

        require(
            block->bitmap[0].load() == 0,
            label,
            "partial bitmap"
        );

        require(
            block->registered.load() == first,
            label,
            "partial registered"
        );

        require(
            !FLAGS::INSTALLED(block->flags, FLAGS::FLAG_DELETE),
            label,
            "partial delete flag"
        );

        bool threw = false;

        try {
            block->deallocate(pointers[0]);
        } catch (const std::runtime_error&) {
            threw = true;
        }

        require(
            threw,
            label,
            "partial double free not detected"
        );

        for (std::size_t i = first; i < count; ++i) {
            pointers[i] = block->allocate();

            require(
                valid_slot(*block, pointers[i]),
                label,
                "late slot or owner"
            );

            require(
                slot_index(*block, pointers[i]) == i,
                label,
                "unexpected slot reuse"
            );
        }

        require(
            block->registered.load() == count,
            label,
            "registered after late allocations"
        );

        require(
            block->allocate() == nullptr,
            label,
            "late overflow"
        );

        for (std::size_t i = first; i < count; ++i) {
            auto* retired = block->deallocate(pointers[i]);

            require(
                retired == (i + 1 == count ? block.get() : nullptr),
                label,
                "late retirement"
            );
        }

        require(
            block->bitmap[0].load() == 0,
            label,
            "final partial bitmap"
        );

        require(
            FLAGS::INSTALLED(block->flags, FLAGS::FLAG_DELETE),
            label,
            "final partial flag"
        );
    }

    template<typename T>
    void test_concurrent_double_free(std::string_view label) {
        auto block = std::make_unique<SlotBlock<T>>();

        std::array<T*, SlotBlock<T>::count> pointers{};

        for (auto& ptr : pointers) {
            ptr = block->allocate();
        }

        std::barrier start(3);

        std::atomic<int> successful{0};
        std::atomic<int> detected{0};
        std::atomic<int> unexpected{0};

        std::vector<std::jthread> threads;

        for (int i = 0; i < 2; ++i) {
            threads.emplace_back([&] {
                start.arrive_and_wait();

                try {
                    if (block->deallocate(pointers[0]) == nullptr) {
                        successful.fetch_add(1);
                    } else {
                        unexpected.fetch_add(1);
                    }
                } catch (const std::runtime_error&) {
                    detected.fetch_add(1);
                } catch (...) {
                    unexpected.fetch_add(1);
                }
            });
        }

        start.arrive_and_wait();
        threads.clear();

        require(
            successful.load() == 1,
            label,
            "concurrent free success count"
        );

        require(
            detected.load() == 1,
            label,
            "concurrent double free not detected"
        );

        require(
            unexpected.load() == 0,
            label,
            "unexpected concurrent free result"
        );

        require(
            block->bitmap[0].load() == UINT32_MAX - 1,
            label,
            "bitmap after concurrent double free"
        );

        require(
            !FLAGS::INSTALLED(block->flags, FLAGS::FLAG_DELETE),
            label,
            "early delete after concurrent double free"
        );

        for (std::size_t i = 1; i < pointers.size(); ++i) {
            auto* retired = block->deallocate(pointers[i]);

            require(
                retired == (
                    i + 1 == pointers.size()
                        ? block.get()
                        : nullptr
                ),
                label,
                "retirement after double free"
            );
        }

        require(
            FLAGS::INSTALLED(block->flags, FLAGS::FLAG_DELETE),
            label,
            "missing final delete flag"
        );
    }

    enum class Mode {
        Batched,
        Interleaved
    };

    template<typename T>
    void test_parallel(
        std::string_view label,
        std::size_t rounds,
        std::size_t workers,
        Mode mode
    ) {
        constexpr auto count = SlotBlock<T>::count;

        std::barrier phase(
            static_cast<std::ptrdiff_t>(workers + 1)
        );

        std::array<T*, count> pointers{};

        std::atomic<std::size_t> allocations{0};
        std::atomic<std::size_t> frees{0};
        std::atomic<std::size_t> retired{0};
        std::atomic<std::size_t> errors{0};
        std::atomic<std::uint32_t> seen_slots{0};

        SlotBlock<T>* current = nullptr;

        std::vector<std::jthread> threads;
        threads.reserve(workers);

        for (std::size_t id = 0; id < workers; ++id) {
            threads.emplace_back([&, id] {
                for (std::size_t round = 0; round < rounds; ++round) {
                    phase.arrive_and_wait();

                    auto* block = current;

                    try {
                        bool exhausted = false;

                        for (
                            std::size_t attempt = 0;
                            attempt < count + workers + 1;
                            ++attempt
                        ) {
                            auto* ptr = block->allocate();

                            if (!ptr) {
                                exhausted = true;
                                break;
                            }

                            const auto position = allocations.fetch_add(
                                1,
                                std::memory_order_relaxed
                            );

                            if (
                                !valid_slot(*block, ptr) ||
                                position >= count
                            ) {
                                errors.fetch_add(
                                    1,
                                    std::memory_order_relaxed
                                );
                                continue;
                            }

                            const auto idx = slot_index(*block, ptr);
                            const auto mask = std::uint32_t{1} << idx;

                            if (
                                seen_slots.fetch_or(
                                    mask,
                                    std::memory_order_relaxed
                                ) & mask
                            ) {
                                errors.fetch_add(
                                    1,
                                    std::memory_order_relaxed
                                );
                                continue;
                            }

                            if (mode == Mode::Batched) {
                                pointers[position] = ptr;
                            } else {
                                write_pattern(ptr, position, round);

                                if ((position + id + round) % 3 == 0) {
                                    std::this_thread::yield();
                                }

                                if (
                                    !verify_pattern(
                                        ptr,
                                        position,
                                        round
                                    )
                                ) {
                                    errors.fetch_add(
                                        1,
                                        std::memory_order_relaxed
                                    );
                                }

                                auto* result = block->deallocate(ptr);

                                frees.fetch_add(
                                    1,
                                    std::memory_order_relaxed
                                );

                                if (result) {
                                    if (result != block) {
                                        errors.fetch_add(
                                            1,
                                            std::memory_order_relaxed
                                        );
                                    }

                                    retired.fetch_add(
                                        1,
                                        std::memory_order_relaxed
                                    );
                                }
                            }
                        }

                        if (!exhausted) {
                            errors.fetch_add(
                                1,
                                std::memory_order_relaxed
                            );
                        }
                    } catch (...) {
                        errors.fetch_add(
                            1,
                            std::memory_order_relaxed
                        );
                    }

                    phase.arrive_and_wait();
                    phase.arrive_and_wait();

                    if (mode == Mode::Batched) {
                        for (
                            std::size_t i = id;
                            i < count;
                            i += workers
                        ) {
                            auto* ptr = pointers[i];

                            if (!ptr) {
                                errors.fetch_add(
                                    1,
                                    std::memory_order_relaxed
                                );
                                continue;
                            }

                            try {
                                if (
                                    !verify_pattern(
                                        ptr,
                                        i,
                                        round
                                    )
                                ) {
                                    errors.fetch_add(
                                        1,
                                        std::memory_order_relaxed
                                    );
                                }

                                auto* result = block->deallocate(ptr);

                                frees.fetch_add(
                                    1,
                                    std::memory_order_relaxed
                                );

                                if (result) {
                                    if (result != block) {
                                        errors.fetch_add(
                                            1,
                                            std::memory_order_relaxed
                                        );
                                    }

                                    retired.fetch_add(
                                        1,
                                        std::memory_order_relaxed
                                    );
                                }
                            } catch (...) {
                                errors.fetch_add(
                                    1,
                                    std::memory_order_relaxed
                                );
                            }
                        }
                    }

                    phase.arrive_and_wait();
                }
            });
        }

        std::size_t failed_rounds = 0;
        std::string first_failure;

        for (std::size_t round = 0; round < rounds; ++round) {
            auto block = std::make_unique<SlotBlock<T>>();

            current = block.get();
            pointers.fill(nullptr);

            allocations.store(0, std::memory_order_relaxed);
            frees.store(0, std::memory_order_relaxed);
            retired.store(0, std::memory_order_relaxed);
            errors.store(0, std::memory_order_relaxed);
            seen_slots.store(0, std::memory_order_relaxed);

            phase.arrive_and_wait();
            phase.arrive_and_wait();

            bool ok =
                allocations.load() == count &&
                errors.load() == 0;

            ok = ok && seen_slots.load() == UINT32_MAX;

            if (mode == Mode::Batched) {
                std::array<bool, count> seen{};

                for (std::size_t i = 0; i < count; ++i) {
                    auto* ptr = pointers[i];

                    if (!valid_slot(*block, ptr)) {
                        ok = false;
                        continue;
                    }

                    const auto idx = slot_index(*block, ptr);

                    if (seen[idx]) {
                        ok = false;
                        pointers[i] = nullptr;
                        continue;
                    }

                    seen[idx] = true;
                    write_pattern(ptr, i, round);
                }

                for (const auto found : seen) {
                    if (!found) {
                        ok = false;
                    }
                }

                ok = ok && block->bitmap[0].load() == UINT32_MAX;

                ok = ok && !FLAGS::INSTALLED(
                    block->flags,
                    FLAGS::FLAG_DELETE
                );
            }

            phase.arrive_and_wait();
            phase.arrive_and_wait();

            ok = ok && errors.load() == 0;
            ok = ok && allocations.load() == count;
            ok = ok && frees.load() == count;
            ok = ok && retired.load() == 1;
            ok = ok && block->offset.load() == count;
            ok = ok && block->registered.load() == count;
            ok = ok && block->bitmap[0].load() == 0;
            ok = ok && block->cur_bit.load() == 0;
            ok = ok && block->cur_map.load() == 0;

            ok = ok && FLAGS::INSTALLED(
                block->flags,
                FLAGS::FLAG_DELETE
            );

            for (const auto& slot : block->memory) {
                if (slot.owner != block.get()) {
                    ok = false;
                }
            }

            if (!ok) {
                if (failed_rounds == 0) {
                    first_failure =
                        "round " + std::to_string(round + 1) +
                        ", allocations=" + std::to_string(allocations.load()) +
                        ", frees=" + std::to_string(frees.load()) +
                        ", retired=" + std::to_string(retired.load()) +
                        ", errors=" + std::to_string(errors.load()) +
                        ", seen=" + std::to_string(seen_slots.load()) +
                        ", bitmap=" + std::to_string(block->bitmap[0].load()) +
                        ", offset=" + std::to_string(block->offset.load()) +
                        ", registered=" + std::to_string(block->registered.load()) +
                        ", flags=" + std::to_string(block->flags.load());
                }

                ++failed_rounds;
            }
        }

        threads.clear();

        require(
            failed_rounds == 0,
            label,
            std::string(
                mode == Mode::Batched
                    ? "batched"
                    : "interleaved"
            ) +
            " parallel test failed: " + first_failure +
            " (failed rounds: " + std::to_string(failed_rounds) + ")"
        );
    }

    template<typename T>
    void run_variant(
        std::string_view label,
        std::size_t rounds,
        std::size_t workers
    ) {
        test_sequential<T>(label);
        test_partial<T>(label);
        test_concurrent_double_free<T>(label);

        test_parallel<T>(
            label,
            rounds,
            workers,
            Mode::Batched
        );

        test_parallel<T>(
            label,
            rounds,
            workers,
            Mode::Interleaved
        );

        std::cout
            << label
            << ": PASS ("
            << rounds
            << " stress rounds, "
            << workers
            << " threads)\n";
    }

    template<std::size_t N>
    void run_size(
        std::size_t rounds,
        std::size_t workers
    ) {
        static_assert(sizeof(Payload<N>) == N);
        static_assert(sizeof(AlignedPayload<N>) == N);

        const auto prefix =
            std::string("SlotBlock ") +
            std::to_string(N) +
            "B";

        run_variant<Payload<N>>(
            prefix + " / plain",
            rounds,
            workers
        );

        run_variant<AlignedPayload<N>>(
            prefix + " / aligned",
            rounds,
            workers
        );
    }

}

inline void test_slotblock(
    std::size_t rounds = 256,
    std::size_t workers = 8
) {
    if (rounds == 0 || workers == 0) {
        throw std::invalid_argument(
            "test_slotblock: rounds and workers must be positive"
        );
    }

    slotblock_tests::run_size<1>(rounds, workers);
    slotblock_tests::run_size<8>(rounds, workers);
    slotblock_tests::run_size<32>(rounds, workers);
    slotblock_tests::run_size<128>(rounds, workers);
    slotblock_tests::run_size<512>(rounds, workers);
    slotblock_tests::run_size<2048>(rounds, workers);
    slotblock_tests::run_size<4096>(rounds, workers);

    std::cout
        << "\nFull SlotBlock Test: PASS\n"
        << "Sizes: 7\n"
        << "Type variants: 14\n"
        << "Stress rounds per variant: " << rounds << '\n'
        << "Worker threads: " << workers << '\n';
}
