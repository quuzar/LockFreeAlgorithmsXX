#include <test/allocator_test.hpp>
#include <test/lockfreestack_test.hpp>
#include <test/benchmark.hpp>
#include <test/stres_test.hpp>

#include <iostream>
int main() {
	std::cout << "START TEST" << std::endl;
	allocator_test();
	stack_test();
	std::cout << "END TEST\n" << std::endl;

	std::cout << "========================================\n";
	std::cout << "Benchmark started" << std::endl;
	std::cout << "========================================\n";
	run_benchmarks();


	std::cout << "========================================\n";
	std::cout << "Stres test started" << std::endl;
	std::cout << "========================================\n";
	
	test();
}

