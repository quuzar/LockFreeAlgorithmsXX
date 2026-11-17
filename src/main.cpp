#include <allocator_test.hpp>
#include <alloc/ArenaAllocator.hpp>

int main() {
    test_allocator<ArenaAllocator>();
    return 0;

}