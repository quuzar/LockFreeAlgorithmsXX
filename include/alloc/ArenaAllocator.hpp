#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <stack>
#include <thread>
#include <vector>

template<typename T>
struct Block;

template<typename T>
struct Slot {
    alignas(T) std::byte storage[sizeof(T)];
    Block<T>* owner = nullptr;
};

template<typename T>
struct Block {
    Slot<T>* memory = nullptr;
    size_t capacity = sizeof(Slot<T>) * 32;

    std::atomic<uint32_t> bitmap{0};
    std::atomic<uint16_t> offset{0};
    std::atomic<uint8_t> registered{0};
    std::atomic_flag delete_flag{};

    Block() {
        memory = new Slot<T>[32];

        for (size_t i = 0; i < 32; ++i) {
            memory[i].owner = this;
        }
    }

    ~Block() {
        delete[] memory;
    }

    Block(const Block&) = delete;
    Block& operator=(const Block&) = delete;

    T* allocate() {
        auto idx = offset.fetch_add(1, std::memory_order_acq_rel);

        if (idx >= 32) {
            offset.fetch_sub(1, std::memory_order_release);
            return nullptr;
        }

        bitmap.fetch_or(uint32_t{1} << idx, std::memory_order_relaxed);
        registered.fetch_add(1, std::memory_order_release);

        return reinterpret_cast<T*>(memory[idx].storage);
    }

    bool deallocate(T* ptr) {
        auto* slot = reinterpret_cast<Slot<T>*>(ptr);
        auto idx = slot - memory;

        auto bit = uint32_t{1} << idx;
        auto old_bitmap = bitmap.fetch_and(~bit, std::memory_order_acq_rel);

        if (old_bitmap == bit &&
            registered.load(std::memory_order_acquire) == 32) {
            return !delete_flag.test_and_set(std::memory_order_acq_rel);
        }

        return false;
    }

    void reset() {
        bitmap.store(0, std::memory_order_relaxed);
        offset.store(0, std::memory_order_relaxed);
        registered.store(0, std::memory_order_relaxed);
        delete_flag.clear(std::memory_order_relaxed);
    }

    bool is_full() {
        return registered.load(std::memory_order_acquire) == 32;
    }

    bool is_empty() {
        return bitmap.load(std::memory_order_acquire) == 0;
    }

    bool can_delete() {
        return delete_flag.test(std::memory_order_acquire);
    }
};

template<typename T>
class ArenaAllocator {
private:
    std::vector<Block<T>*> blocks;
    std::stack<Block<T>*> free_blocks;

    std::atomic<Block<T>*> cur_block{nullptr};
    std::atomic<uint64_t> offset{0};
    std::atomic_flag lock{};

    std::atomic<uint32_t> active_allocations{0};

    void reset_current_block() {
        Block<T>* block = nullptr;

        if (!free_blocks.empty()) {
            block = free_blocks.top();
            free_blocks.pop();

            block->reset();
        } else {
            block = new Block<T>();

            try {
                blocks.push_back(block);
            } catch (...) {
                delete block;
                throw;
            }

            offset.fetch_add(1, std::memory_order_relaxed);
        }

        cur_block.store(block, std::memory_order_release);
    }

public:
    ArenaAllocator() {
        auto* block = new Block<T>();

        try {
            blocks.push_back(block);
        } catch (...) {
            delete block;
            throw;
        }

        cur_block.store(block, std::memory_order_release);
        offset.store(1, std::memory_order_relaxed);
    }

    ~ArenaAllocator() {
        for (auto* block : blocks) {
            delete block;
        }
    }

    ArenaAllocator(const ArenaAllocator&) = delete;
    ArenaAllocator& operator=(const ArenaAllocator&) = delete;

    T* allocate() {
        while (true) {
            if (lock.test(std::memory_order_acquire)) {
                std::this_thread::yield();
                continue;
            }

            active_allocations.fetch_add(1, std::memory_order_seq_cst);

            if (lock.test(std::memory_order_seq_cst)) {
                active_allocations.fetch_sub(1, std::memory_order_release);
                std::this_thread::yield();
                continue;
            }

            auto block_ = cur_block.load(std::memory_order_acquire);
            auto ptr = block_->allocate();

            active_allocations.fetch_sub(1, std::memory_order_release);

            if (ptr == nullptr) {
                if (lock.test_and_set(std::memory_order_seq_cst)) {
                    continue;
                }

                if (cur_block.load(std::memory_order_acquire) != block_) {
                    lock.clear(std::memory_order_release);
                    continue;
                }

                while (active_allocations.load(std::memory_order_seq_cst) != 0) {
                    std::this_thread::yield();
                }

                if (block_->is_full()) {
                    try {
                        reset_current_block();
                    } catch (...) {
                        lock.clear(std::memory_order_release);
                        throw;
                    }
                }

                lock.clear(std::memory_order_release);
                continue;
            }

            return ptr;
        }
    }

    void deallocate(T* ptr) {
        if (ptr == nullptr) {
            return;
        }

        auto* slot = reinterpret_cast<Slot<T>*>(ptr);
        auto* block = slot->owner;

        if (!block->deallocate(ptr)) {
            return;
        }

        while (lock.test_and_set(std::memory_order_seq_cst)) {
            std::this_thread::yield();
        }

        try {
            free_blocks.push(block);
        } catch (...) {
            lock.clear(std::memory_order_release);
            throw;
        }

        lock.clear(std::memory_order_release);
    }

    void reset() {
        while (lock.test_and_set(std::memory_order_seq_cst)) {
            std::this_thread::yield();
        }

        while (active_allocations.load(std::memory_order_seq_cst) != 0) {
            std::this_thread::yield();
        }

        while (!free_blocks.empty()) {
            free_blocks.pop();
        }

        for (size_t i = 1; i < blocks.size(); ++i) {
            delete blocks[i];
        }

        blocks.resize(1);
        blocks[0]->reset();

        cur_block.store(blocks[0], std::memory_order_release);
        offset.store(1, std::memory_order_relaxed);

        lock.clear(std::memory_order_release);
    }
};
