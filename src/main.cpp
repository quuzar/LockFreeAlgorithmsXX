#include <tests/test_block.hpp>
#include <tests/test_slotblock.hpp>

int main() {
    test_full_block(1024, 8);
    test_slotblock(1024, 8);

    return 0;
}