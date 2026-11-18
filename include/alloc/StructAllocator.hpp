#pragma once
#include <cstdint>
#include <cstddef>
#include <bit>

namespace fla::alloc::blocks {
    template<typename T, size_t Size = std::bit_ceil(sizeof(T))>
    struct Block;

    template<typename T>
    struct alignas(4096) Block<T, 1> {
        constexpr static uint8_t    bit_count{57};
        constexpr static uint16_t   count{3632};

        uint16_t    offset;
        uint16_t    registered;
        uint8_t     flags;
        uint8_t     cur_bit{};
        uint16_t    cur_map{};
        uint64_t    bitmap[bit_count]{};

        uint8_t     memory[count];
    };

    template<typename T>
    struct alignas(4096) Block<T, 2> {
        constexpr static uint8_t    bit_count{31};
        constexpr static uint16_t   count{1920};

        uint16_t    offset;
        uint16_t    registered;
        uint8_t     flags;
        uint8_t     cur_bit{};
        uint16_t    cur_map{};
        uint64_t    bitmap[bit_count]{};

        uint16_t    memory[count];
    };

    template<typename T>
    struct alignas(4096) Block<T, 4> {
        constexpr static uint8_t    bit_count{16};
        constexpr static uint16_t   count{990};

        uint16_t    offset;
        uint16_t    registered;
        uint8_t     flags;
        uint8_t     cur_bit{};
        uint16_t    cur_map{};
        uint64_t    bitmap[bit_count]{};

        uint32_t    memory[count];
    };

    template<typename T>
    struct alignas(4096) Block<T, 8> {
        constexpr static uint8_t    bit_count{8};
        constexpr static uint16_t   count{503};

        uint16_t    offset;
        uint16_t    registered;
        uint8_t     flags;
        uint8_t     cur_bit{};
        uint16_t    cur_map{};
        uint64_t    bitmap[bit_count]{};

        uint64_t    memory[count];
    };

    template<typename T>
    struct alignas(4096) Block<T, 16> {
        constexpr static uint8_t    bit_count{4};
        constexpr static uint16_t   count{253};

        uint16_t    offset;
        uint16_t    registered;
        uint8_t     flags;
        uint8_t     cur_bit{};
        uint16_t    cur_map{};
        uint64_t    bitmap[bit_count]{};
        uint8_t     padding[8]{};
        uint64_t    memory[count][2];
    };

    template<typename T>
    struct alignas(4096) Block<T, 32> {
        constexpr static uint8_t    bit_count{2};
        constexpr static uint16_t   count{127};

        uint16_t    offset;
        uint16_t    registered;
        uint8_t     flags;
        uint8_t     cur_bit{};
        uint16_t    cur_map{};
        uint64_t    bitmap[bit_count]{};
        uint8_t     padding[8]{};
        uint64_t    memory[count][4];
    };

    template<typename T>
    struct alignas(4096) Block<T, 64> {
        constexpr static uint8_t    bit_count{1};
        constexpr static uint16_t   count{63};

        uint16_t    offset;
        uint16_t    registered;
        uint8_t     flags;
        uint8_t     cur_bit{};
        uint16_t    cur_map{};
        uint64_t    bitmap[bit_count]{};
        uint8_t     padding[48]{};
        uint64_t    memory[count][8];
    };

    template<typename T>
    struct alignas(4096) Block<T, 128> {
        constexpr static uint8_t    bit_count{1};
        constexpr static uint16_t   count{31};

        uint16_t    offset;
        uint16_t    registered;
        uint8_t     flags;
        uint8_t     cur_bit{};
        uint16_t    cur_map{};
        uint64_t    bitmap[bit_count]{};
        uint8_t     padding[112]{};
        uint64_t    memory[count][16];
    };

    template<typename T>
    struct alignas(4096) Block<T, 256> {
        constexpr static uint8_t    bit_count{1};
        constexpr static uint16_t   count{15};

        uint16_t    offset;
        uint16_t    registered;
        uint8_t     flags;
        uint8_t     cur_bit{};
        uint16_t    cur_map{};
        uint64_t    bitmap[bit_count]{};
        uint8_t     padding[240]{};
        uint64_t    memory[count][32];
    };
}

namespace fla::alloc::blocks {
    template<typename T>
    struct SlotBlock {
        
    };
}