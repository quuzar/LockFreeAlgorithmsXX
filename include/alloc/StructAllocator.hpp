#pragma once
#include <algorithm>
#include <cstdint>
#include <atomic>
#include <cstddef>
#include <bit>
#include <stdexcept>
#include <thread>

// Declaration
namespace alloc::blocks {
    template<typename T>
    struct SlotBlock;

    template<typename T>
    struct Slot;

    template<typename T, size_t Size = std::bit_ceil(sizeof(T))>
    struct Block;

    template<typename T, size_t Size = std::bit_ceil(sizeof(T))>
    static auto ALLOCATE(Block<T, Size>& block) -> T*;

    template<typename T, size_t Size = std::bit_ceil(sizeof(T))>
    static auto DEALLOCATE(Block<T, Size>& block, T* ptr) -> Block<T, Size>*;

    struct FLAGS;

    struct BlockFields {
        template<typename T, size_t Size>
        static auto BIT_COUNT(Block<T, Size> &block) {
            return Block<T, Size>::bit_count;
        }

        template<typename T, size_t Size>
        static auto COUNT(Block<T, Size> &block) {
            return Block<T, Size>::count;
        }

        template<typename T, size_t Size>
        static auto SIZE_BLOCK(Block<T, Size> &block) {
            return Block<T, Size>::size_block;
        }

        template<typename T, size_t Size>
        static auto SIZE_BITMAP(Block<T, Size> &block) {
            return Block<T, Size>::size_bitmap;
        }

        template<typename T, size_t Size>
        static auto& OFFSET(Block<T, Size> &block) {
            if constexpr(Size == 1)         return static_cast<Block<T, 1>&>(block).offset;
            else if constexpr(Size == 2)    return static_cast<Block<T, 2>&>(block).offset;
            else if constexpr(Size == 4)    return static_cast<Block<T, 4>&>(block).offset;
            else if constexpr(Size == 8)    return static_cast<Block<T, 8>&>(block).offset;
            else if constexpr(Size == 16)   return static_cast<Block<T, 16>&>(block).offset;
            else if constexpr(Size == 32)   return static_cast<Block<T, 32>&>(block).offset;
            else if constexpr(Size == 64)   return static_cast<Block<T, 64>&>(block).offset;
            else if constexpr(Size == 128)  return static_cast<Block<T, 128>&>(block).offset;
            else if constexpr(Size == 256)  return static_cast<Block<T, 256>&>(block).offset;
        }

        template<typename T, size_t Size>
        static auto& REGISTERED(Block<T, Size> &block) {
            if constexpr(Size == 1)         return static_cast<Block<T, 1>&>(block).registered;
            else if constexpr(Size == 2)    return static_cast<Block<T, 2>&>(block).registered;
            else if constexpr(Size == 4)    return static_cast<Block<T, 4>&>(block).registered;
            else if constexpr(Size == 8)    return static_cast<Block<T, 8>&>(block).registered;
            else if constexpr(Size == 16)   return static_cast<Block<T, 16>&>(block).registered;
            else if constexpr(Size == 32)   return static_cast<Block<T, 32>&>(block).registered;
            else if constexpr(Size == 64)   return static_cast<Block<T, 64>&>(block).registered;
            else if constexpr(Size == 128)  return static_cast<Block<T, 128>&>(block).registered;
            else if constexpr(Size == 256)  return static_cast<Block<T, 256>&>(block).registered;
        }

        template<typename T, size_t Size>
        static auto& FLAGS(Block<T, Size> &block) {
            if constexpr(Size == 1)         return static_cast<Block<T, 1>&>(block).flags;
            else if constexpr(Size == 2)    return static_cast<Block<T, 2>&>(block).flags;
            else if constexpr(Size == 4)    return static_cast<Block<T, 4>&>(block).flags;
            else if constexpr(Size == 8)    return static_cast<Block<T, 8>&>(block).flags;
            else if constexpr(Size == 16)   return static_cast<Block<T, 16>&>(block).flags;
            else if constexpr(Size == 32)   return static_cast<Block<T, 32>&>(block).flags;
            else if constexpr(Size == 64)   return static_cast<Block<T, 64>&>(block).flags;
            else if constexpr(Size == 128)  return static_cast<Block<T, 128>&>(block).flags;
            else if constexpr(Size == 256)  return static_cast<Block<T, 256>&>(block).flags;
        }

        template<typename T, size_t Size>
        static auto& CUR_BIT(Block<T, Size> &block) {
            if constexpr(Size == 1)         return static_cast<Block<T, 1>&>(block).cur_bit;
            else if constexpr(Size == 2)    return static_cast<Block<T, 2>&>(block).cur_bit;
            else if constexpr(Size == 4)    return static_cast<Block<T, 4>&>(block).cur_bit;
            else if constexpr(Size == 8)    return static_cast<Block<T, 8>&>(block).cur_bit;
            else if constexpr(Size == 16)   return static_cast<Block<T, 16>&>(block).cur_bit;
            else if constexpr(Size == 32)   return static_cast<Block<T, 32>&>(block).cur_bit;
            else if constexpr(Size == 64)   return static_cast<Block<T, 64>&>(block).cur_bit;
            else if constexpr(Size == 128)  return static_cast<Block<T, 128>&>(block).cur_bit;
            else if constexpr(Size == 256)  return static_cast<Block<T, 256>&>(block).cur_bit;
        }

        template<typename T, size_t Size>
        static auto& CUR_MAP(Block<T, Size> &block) {
            if constexpr(Size == 1)         return static_cast<Block<T, 1>&>(block).cur_map;
            else if constexpr(Size == 2)    return static_cast<Block<T, 2>&>(block).cur_map;
            else if constexpr(Size == 4)    return static_cast<Block<T, 4>&>(block).cur_map;
            else if constexpr(Size == 8)    return static_cast<Block<T, 8>&>(block).cur_map;
            else if constexpr(Size == 16)   return static_cast<Block<T, 16>&>(block).cur_map;
            else if constexpr(Size == 32)   return static_cast<Block<T, 32>&>(block).cur_map;
            else if constexpr(Size == 64)   return static_cast<Block<T, 64>&>(block).cur_map;
            else if constexpr(Size == 128)  return static_cast<Block<T, 128>&>(block).cur_map;
            else if constexpr(Size == 256)  return static_cast<Block<T, 256>&>(block).cur_map;
        }

        template<typename T, size_t Size>
        static auto BITMAP(Block<T, Size> &block) {
            if constexpr(Size == 1)         return static_cast<Block<T, 1>&>(block).bitmap;
            else if constexpr(Size == 2)    return static_cast<Block<T, 2>&>(block).bitmap;
            else if constexpr(Size == 4)    return static_cast<Block<T, 4>&>(block).bitmap;
            else if constexpr(Size == 8)    return static_cast<Block<T, 8>&>(block).bitmap;
            else if constexpr(Size == 16)   return static_cast<Block<T, 16>&>(block).bitmap;
            else if constexpr(Size == 32)   return static_cast<Block<T, 32>&>(block).bitmap;
            else if constexpr(Size == 64)   return static_cast<Block<T, 64>&>(block).bitmap;
            else if constexpr(Size == 128)  return static_cast<Block<T, 128>&>(block).bitmap;
            else if constexpr(Size == 256)  return static_cast<Block<T, 256>&>(block).bitmap;
        }

        template<typename T, size_t Size>
        static auto MEMORY(Block<T, Size> &block) {
            if constexpr(Size == 1)         return reinterpret_cast<std::byte*>(static_cast<Block<T, 1>&>(block).memory);
            else if constexpr(Size == 2)    return reinterpret_cast<std::byte*>(static_cast<Block<T, 2>&>(block).memory);
            else if constexpr(Size == 4)    return reinterpret_cast<std::byte*>(static_cast<Block<T, 4>&>(block).memory);
            else if constexpr(Size == 8)    return reinterpret_cast<std::byte*>(static_cast<Block<T, 8>&>(block).memory);
            else if constexpr(Size == 16)   return reinterpret_cast<std::byte*>(static_cast<Block<T, 16>&>(block).memory);
            else if constexpr(Size == 32)   return reinterpret_cast<std::byte*>(static_cast<Block<T, 32>&>(block).memory);
            else if constexpr(Size == 64)   return reinterpret_cast<std::byte*>(static_cast<Block<T, 64>&>(block).memory);
            else if constexpr(Size == 128)  return reinterpret_cast<std::byte*>(static_cast<Block<T, 128>&>(block).memory);
            else if constexpr(Size == 256)  return reinterpret_cast<std::byte*>(static_cast<Block<T, 256>&>(block).memory);
        }
    };
};

// Blocks
namespace alloc::blocks {
    struct FLAGS {
        static constexpr uint8_t FLAG_DELETE {1};
        static constexpr uint8_t FLAG_BLOCK { uint8_t{1} << 1 };

        static void SET(std::atomic<uint8_t>& flags, const uint8_t flag) {
            flags.fetch_or(flag, std::memory_order_release);
        }

        static void REMOVE(std::atomic<uint8_t>& flags, const uint8_t flag) {
            flags.fetch_and(~flag, std::memory_order_release);
        }

        static bool LOCK(std::atomic<uint8_t>& flags, const uint8_t flag) {
            return  (flags.fetch_or(flag, std::memory_order_acquire) & flag) != 0;
        }

        static bool INSTALLED(std::atomic<uint8_t>& flags, const uint8_t flag) {
            return (flags.load(std::memory_order_acquire) & flag) != 0;
        }
    };
}

// Flags
namespace alloc::blocks {
    template<typename T>
    struct alignas(4096) Block<T, 1> {
        constexpr static uint16_t   bit_count{57};
        constexpr static uint16_t   count{3632};
        constexpr static uint16_t   size_block{1};
        constexpr static uint16_t   size_bitmap{64};

        std::atomic<uint16_t>   offset{};
        std::atomic<uint16_t>   registered{};
        std::atomic<uint8_t>    flags{};
        std::atomic<uint8_t>    cur_bit{};
        std::atomic<uint16_t>   cur_map{};
        std::atomic<uint64_t>   bitmap[bit_count]{};

        uint8_t                 memory[count]{};

        auto allocate() {
            return ALLOCATE(*this);
        }

        auto deallocate(T* ptr) {
            return DEALLOCATE(*this, ptr);
        }
    };

    template<typename T>
    struct alignas(4096) Block<T, 2> {
        constexpr static uint8_t    bit_count{31};
        constexpr static uint16_t   count{1920};
        constexpr static uint16_t   size_block{2};
        constexpr static uint16_t   size_bitmap{64};

        std::atomic<uint16_t>   offset{};
        std::atomic<uint16_t>   registered{};
        std::atomic<uint8_t>    flags{};
        std::atomic<uint8_t>    cur_bit{};
        std::atomic<uint16_t>   cur_map{};
        std::atomic<uint64_t>   bitmap[bit_count]{};

        uint16_t                memory[count];

        auto allocate() {
            return ALLOCATE(*this);
        }

        auto deallocate(T* ptr) {
            return DEALLOCATE(*this, ptr);
        }
    };

    template<typename T>
    struct alignas(4096) Block<T, 4> {
        constexpr static uint16_t   bit_count{16};
        constexpr static uint16_t   count{990};
        constexpr static uint16_t   size_block{4};
        constexpr static uint16_t   size_bitmap{64};

        std::atomic<uint16_t>   offset{};
        std::atomic<uint16_t>   registered{};
        std::atomic<uint8_t>    flags{};
        std::atomic<uint8_t>    cur_bit{};
        std::atomic<uint16_t>   cur_map{};
        std::atomic<uint64_t>   bitmap[bit_count]{};

        uint32_t                memory[count];

        auto allocate() {
            return ALLOCATE(*this);
        }

        auto deallocate(T* ptr) {
            return DEALLOCATE(*this, ptr);
        }
    };

    template<typename T>
    struct alignas(4096) Block<T, 8> {
        constexpr static uint16_t   bit_count{8};
        constexpr static uint16_t   count{503};
        constexpr static uint16_t   size_block{8};
        constexpr static uint16_t   size_bitmap{64};

        std::atomic<uint16_t>   offset{};
        std::atomic<uint16_t>   registered{};
        std::atomic<uint8_t>    flags{};
        std::atomic<uint8_t>    cur_bit{};
        std::atomic<uint16_t>   cur_map{};
        std::atomic<uint64_t>   bitmap[bit_count]{};

        uint64_t                memory[count];

        auto allocate() {
            return ALLOCATE(*this);
        }

        auto deallocate(T* ptr) {
            return DEALLOCATE(*this, ptr);
        }
    };

    template<typename T>
    struct alignas(4096) Block<T, 16> {
        constexpr static uint16_t   bit_count{4};
        constexpr static uint16_t   count{253};
        constexpr static uint16_t   size_block{16};
        constexpr static uint16_t   size_bitmap{64};

        std::atomic<uint16_t>   offset{};
        std::atomic<uint16_t>   registered{};
        std::atomic<uint8_t>    flags{};
        std::atomic<uint8_t>    cur_bit{};
        std::atomic<uint16_t>   cur_map{};
        std::atomic<uint64_t>   bitmap[bit_count]{};
        uint8_t                 padding[8]{};
        uint64_t                memory[count][size_block / sizeof(uint64_t)];

        auto allocate() {
            return ALLOCATE(*this);
        }

        auto deallocate(T* ptr) {
            return DEALLOCATE(*this, ptr);
        }
    };

    template<typename T>
    struct alignas(4096) Block<T, 32> {
        constexpr static uint16_t   bit_count{2};
        constexpr static uint16_t   count{127};
        constexpr static uint16_t   size_block{32};
        constexpr static uint16_t   size_bitmap{64};

        std::atomic<uint16_t>   offset{};
        std::atomic<uint16_t>   registered{};
        std::atomic<uint8_t>    flags{};
        std::atomic<uint8_t>    cur_bit{};
        std::atomic<uint16_t>   cur_map{};
        std::atomic<uint64_t>   bitmap[bit_count]{};
        uint8_t                 padding[8]{};
        uint64_t                memory[count][size_block / sizeof(uint64_t)];

        auto allocate() {
            return ALLOCATE(*this);
        }

        auto deallocate(T* ptr) {
            return DEALLOCATE(*this, ptr);
        }
    };

    template<typename T>
    struct alignas(4096) Block<T, 64> {
        constexpr static uint16_t   bit_count{1};
        constexpr static uint16_t   count{63};
        constexpr static uint16_t   size_block{64};
        constexpr static uint16_t   size_bitmap{64};

        std::atomic<uint16_t>   offset{};
        std::atomic<uint16_t>   registered{};
        std::atomic<uint8_t>    flags{};
        std::atomic<uint8_t>    cur_bit{};
        std::atomic<uint16_t>   cur_map{};
        std::atomic<uint64_t>   bitmap[bit_count]{};
        uint8_t                 padding[48]{};
        uint64_t                memory[count][size_block / sizeof(uint64_t)]{};

        auto allocate() {
            return ALLOCATE(*this);
        }

        auto deallocate(T* ptr) {
            return DEALLOCATE(*this, ptr);
        }
    };

    template<typename T>
    struct alignas(4096) Block<T, 128> {
        constexpr static uint8_t    bit_count{1};
        constexpr static uint16_t   count{31};
        constexpr static uint16_t   size_block{128};
        constexpr static uint16_t   size_bitmap{64};

        std::atomic<uint16_t>   offset{};
        std::atomic<uint16_t>   registered{};
        std::atomic<uint8_t>    flags{};
        std::atomic<uint8_t>    cur_bit{};
        std::atomic<uint16_t>   cur_map{};
        std::atomic<uint64_t>   bitmap[bit_count]{};
        uint8_t     padding[112]{};
        uint64_t    memory[count][size_block / sizeof(uint64_t)]{};

        auto allocate() {
            return ALLOCATE(*this);
        }

        auto deallocate(T* ptr) {
            return DEALLOCATE(*this, ptr);
        }
    };

    template<typename T>
    struct alignas(4096) Block<T, 256> {
        constexpr static uint16_t   bit_count{1};
        constexpr static uint16_t   count{15};
        constexpr static uint16_t   size_block{256};
        constexpr static uint16_t   size_bitmap{64};

        std::atomic<uint16_t>   offset{};
        std::atomic<uint16_t>   registered{};
        std::atomic<uint8_t>    flags{};
        std::atomic<uint8_t>    cur_bit{};
        std::atomic<uint16_t>   cur_map{};
        std::atomic<uint64_t>   bitmap[bit_count]{};
        uint8_t     padding[240]{};
        uint64_t    memory[count][size_block / sizeof(uint64_t)]{};

        auto allocate() {
            return ALLOCATE(*this);
        }

        auto deallocate(T* ptr) {
            return DEALLOCATE(*this, ptr);
        }
    };
}

// SlotBlock
namespace alloc::blocks {
    template<typename T>
    struct Slot {
        alignas(T) uint8_t memory[sizeof(T)];
        SlotBlock<T>* owner;
    };

    template<typename T>
    struct SlotBlock {
        constexpr static uint8_t    bit_count{1};
        constexpr static uint16_t   count{32};

        std::atomic<uint16_t>   offset{};
        std::atomic<uint16_t>   registered{};
        std::atomic<uint8_t>    flags{};
        std::atomic<uint8_t>    cur_bit{};
        std::atomic<uint16_t>   cur_map{};
        std::atomic<uint32_t>   bitmap[bit_count]{};
        Slot<T>                 memory[count]{};

        SlotBlock() {
            for (auto& slot : memory) {
                slot.owner = this;
            }
        }

        auto allocate() -> T*{
            const auto idx = offset.fetch_add(1, std::memory_order_acq_rel);

            if (idx >= count) {
                offset.fetch_sub(1, std::memory_order_release);
                return nullptr;
            }

            const auto idx_bitset = uint32_t{1} << idx;

            bitmap[0].fetch_or(idx_bitset, std::memory_order_relaxed);
            registered.fetch_add(1, std::memory_order_release);

            return reinterpret_cast<T*>(memory[idx].memory);
        }

        auto deallocate(T* ptr) -> SlotBlock<T>* {
            const auto idx = (
                reinterpret_cast<std::uintptr_t>(ptr) -
                reinterpret_cast<std::uintptr_t>(memory)
            ) / sizeof(Slot<T>);

            const auto idx_bitset = uint32_t{1} << idx;
            const auto old_bitmap = bitmap->fetch_and(~idx_bitset, std::memory_order_acq_rel);

            if ((old_bitmap & idx_bitset) == 0) {
                throw std::runtime_error("double free");
            }

            if ((old_bitmap & ~idx_bitset) != 0) {
                return nullptr;
            }

            const auto cur_regist = registered.load(std::memory_order_acquire);
            const auto cur_bitmap = bitmap->load(std::memory_order_acquire);

            if (cur_bitmap == 0 && cur_regist == count) {
                const auto old_flags = flags.fetch_or(
                    FLAGS::FLAG_DELETE,
                    std::memory_order_acq_rel
                );

                if ((old_flags & FLAGS::FLAG_DELETE) == 0) {
                    return this;
                }
            }

            return nullptr;
        }
    };
}

// Allocate/Deallocate
namespace alloc::blocks {
    template<typename T, size_t Size>
    auto ALLOCATE(Block<T, Size> &block) -> T* {
        using BF = BlockFields;

        const auto idx = BF::OFFSET(block).fetch_add(1, std::memory_order_acq_rel);
        const auto idx_bitmap = idx / BF::SIZE_BITMAP(block);
        const auto idx_bitset = idx % BF::SIZE_BITMAP(block);

        if (idx >= BF::COUNT(block)) {
            BF::OFFSET(block).fetch_sub(1, std::memory_order_release);
            return nullptr;
        }

        auto ptr = reinterpret_cast<T*>(
           BF::MEMORY(block) + idx * BF::SIZE_BLOCK(block)
        );
        auto bitset = uint64_t{1} << idx_bitset;

        BF::BITMAP(block)[idx_bitmap].fetch_or(bitset, std::memory_order_relaxed);
        BF::REGISTERED(block).fetch_add(1, std::memory_order_release);

        return ptr;
    };

    template<typename T, size_t Size>
    auto DEALLOCATE(Block<T, Size> &block, T* ptr) -> Block<T, Size>* {
        using BF = BlockFields;

        const auto idx = (
            reinterpret_cast<uint64_t>(ptr) -
            reinterpret_cast<uint64_t>(BF::MEMORY(block))
        ) / BF::SIZE_BLOCK(block);

        const auto idx_bitmap = idx / BF::SIZE_BITMAP(block);
        const auto idx_bitset = idx % BF::SIZE_BITMAP(block);
        const auto bitset = uint64_t{1} << idx_bitset;
        const auto old_bitmap =
            BF::BITMAP(block)[idx_bitmap].fetch_and(~bitset, std::memory_order_acq_rel);

        if ((old_bitmap & bitset) == 0)
            throw std::runtime_error("double free");

        if ((old_bitmap & ~bitset) != 0)
            return nullptr;

        while (FLAGS::LOCK(BF::FLAGS(block), FLAGS::FLAG_BLOCK)) {
            std::this_thread::yield();
        }

        if (FLAGS::INSTALLED(BF::FLAGS(block), FLAGS::FLAG_DELETE)) {
            FLAGS::REMOVE(BF::FLAGS(block), FLAGS::FLAG_BLOCK);
            return nullptr;
        }

        auto const reg = BF::REGISTERED(block).load(std::memory_order_acquire);

        if (reg != BF::COUNT(block)) {
            FLAGS::REMOVE(BF::FLAGS(block), FLAGS::FLAG_BLOCK);
            return nullptr;
        }

        if constexpr (Block<T, Size>::bit_count == 1) {
            if (BF::BITMAP(block)[0].load(std::memory_order_acquire) == 0) {
                FLAGS::SET(
                    BF::FLAGS(block), FLAGS::FLAG_DELETE
                    );
                FLAGS::REMOVE(
                    BF::FLAGS(block), FLAGS::FLAG_BLOCK
                    );

                return &block;
            }

            FLAGS::REMOVE(BF::FLAGS(block), FLAGS::FLAG_BLOCK);
            return nullptr;
        }
        else {
            const auto& bitmap = BF::BITMAP(block);
            for (int i = 0; i < BF::BIT_COUNT(block); i++) {
                if (bitmap[i].load(std::memory_order_acquire) != 0) {
                    FLAGS::REMOVE(BF::FLAGS(block), FLAGS::FLAG_BLOCK);
                    return nullptr;
                }
            }

            FLAGS::SET(BF::FLAGS(block), FLAGS::FLAG_DELETE);
            FLAGS::REMOVE(BF::FLAGS(block), FLAGS::FLAG_BLOCK);

            return &block;
        }
    }
}