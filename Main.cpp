#include <iostream>
#include <random>
#include <thread>
#define NOMINMAX
#include <Windows.h>
#include "LayeredHashMap.h"
#include <concurrent_unordered_map.h>
#include <tbb/concurrent_unordered_map.h>

static thread_local std::mt19937 randomGen;

// Just to check if the values are properly written
#define MAGIC_VAL	123456789

// Max Size for random String generation functions
static const unsigned int Size = 85;

// For sequential integer generation.
static unsigned int Cnt = 0;

inline std::string GenerateVarLenStr() {
	// Generate a string of length <= Size.
	char randomStr[Size + 1] = { 0 };
	for (int i = 0; i < Size; i++) {
		auto random = randomGen();
		if (!(randomStr[i] = char(random >> 24)))
			break;
	}
	return std::string(randomStr);
}

inline std::string GenerateFixedLenStr() {
	// Generate a string of length == size.
	char randomStr[Size + 1] = { 0 };
	for (int i = 1; i < Size; i++) {
		auto random = (randomGen() >> 25); 
		randomStr[i] = char(1 + random);
	}
	return std::string(randomStr);
}

inline unsigned int GenerateRandomInt() {
	return randomGen();
}

inline unsigned int GenerateSeqInt() {
	return ++Cnt;
}

template<class T>
double BenchLayeredHashMap(const size_t Iterations, const size_t ThreadCount, const std::vector<T>& Precomputed) {
	std::vector<std::thread> Threads(ThreadCount);
	LARGE_INTEGER Begin, End, Frequency;
	QueryPerformanceCounter(&Begin);
	LayeredHashMap<T, size_t> shm(Iterations);
	for (size_t i = 0U; i < ThreadCount; i++) {
		Threads[i] = std::thread([=, &shm]() {
			for (auto j = i; j < Iterations; j += ThreadCount) {
				shm.Write(Precomputed[j], MAGIC_VAL);
				if (shm.Read(Precomputed[j]) != MAGIC_VAL) {
					std::cout << "Read Error on key = " << j << "\n";
				}
			}
		});
	}
	for (auto i = 0U; i < ThreadCount; i++) {
		Threads[i].join();
	}
	QueryPerformanceCounter(&End);
	QueryPerformanceFrequency(&Frequency);
	return (End.QuadPart - Begin.QuadPart) / (Frequency.QuadPart + 0.0);
}

template<class T>
double BenchTbbUnorderedMap(const size_t Iterations, const size_t ThreadCount, const std::vector<T>& Precomputed) {
	std::vector<std::thread> Threads(ThreadCount);
	LARGE_INTEGER Begin, End, Frequency;
	QueryPerformanceCounter(&Begin);
	tbb::concurrent_unordered_map<T, size_t> shm;
	for (size_t i = 0U; i < ThreadCount; i++) {
		Threads[i] = std::thread([=, &shm]() {
			for (auto j = i; j < Iterations; j += ThreadCount) {
				shm[Precomputed[j]] = MAGIC_VAL;
				if (shm[Precomputed[j]] != MAGIC_VAL) {
					std::cout << "Read Error on key = " << j << "\n";
				}
			}
		});
	}
	for (auto i = 0U; i < ThreadCount; i++) {
		Threads[i].join();
	}
	QueryPerformanceCounter(&End);
	QueryPerformanceFrequency(&Frequency);
	return (End.QuadPart - Begin.QuadPart) / (Frequency.QuadPart + 0.0);
}

template<class T>
double BenchConcurrentUnorderedMap(const size_t Iterations, const size_t ThreadCount, const std::vector<T>& Precomputed) {
	std::vector<std::thread> Threads(ThreadCount);
	LARGE_INTEGER Begin, End, Frequency;
	QueryPerformanceCounter(&Begin);
	Concurrency::concurrent_unordered_map<T, size_t> shm;
	for (size_t i = 0U; i < ThreadCount; i++) {
		Threads[i] = std::thread([=, &shm]() {
			for (auto j = i; j < Iterations; j += ThreadCount) {
				shm[Precomputed[j]] = MAGIC_VAL;
				if (shm[Precomputed[j]] != MAGIC_VAL) {
					std::cout << "Read Error on key = " << j << "\n";
				}
			}
		});
	}
	for (auto i = 0U; i < ThreadCount; i++) {
		Threads[i].join();
	}
	QueryPerformanceCounter(&End);
	QueryPerformanceFrequency(&Frequency);
	return (End.QuadPart - Begin.QuadPart) / (Frequency.QuadPart + 0.0);
}

int main() {
	// Type of Key to bench
	using T = std::string;
	// Function to use to generate the Keys : strings can be of variable or of fixed size, integers can be random or sequential...
	auto Func = GenerateFixedLenStr;
	// Number of keys to insert inside the table
	auto ElementCount = Primes[13];
	// Number of threads to use for the benchmark
	auto ThreadCount = 3;
	// Number of times the benchmark will be repeated.
	auto NumberOfTries = 25;
	// Fill a vector with precomputed keys, so the keys will be computed only once.
	std::vector<T> Precomputed(ElementCount);
	for (size_t i = 0; i < ElementCount; ++i)
		Precomputed[i] = Func();
	// Bench the 3 solutions.
	double Layered = 0., Concurrent = 0., Tbb = 0.;
	for (auto Try = 0; Try < NumberOfTries; Try++) {
		Layered += BenchLayeredHashMap<T>(ElementCount, ThreadCount, Precomputed);
	}
	for (auto Try = 0; Try < NumberOfTries; Try++) {
		Concurrent += BenchConcurrentUnorderedMap<T>(ElementCount, ThreadCount, Precomputed);
	}
	for (auto Try = 0; Try < NumberOfTries; Try++) {
		Tbb += BenchTbbUnorderedMap<T>(ElementCount, ThreadCount, Precomputed);
	}
	// Precision of the result : 10^-2 s
	Layered = ceil(Layered * 100.) / 100.;
	Concurrent = ceil(Concurrent * 100.) / 100.;
	Tbb = ceil(Tbb * 100.) / 100.;
	// Display the results
	std::cout << NumberOfTries << " iterations completed; " << ElementCount << " elements of type\n\t " 
			  << typeid(T).name() 
			  << " \ninserted with " << ThreadCount << " threads\n";
	std::cout << "LayeredHashMap:           " << Layered / NumberOfTries	<< " s\n";
	std::cout << "Microsoft Concurrency:    " << Concurrent / NumberOfTries << " s\n";
	std::cout << "Intel TBB:                " << Tbb / NumberOfTries		<< " s\n";
	(void)getchar();
}




