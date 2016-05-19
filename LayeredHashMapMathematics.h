/*
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

Created by: Matthieu Pinard, Ecole des Mines de Saint-Etienne, matthieu.pinard@etu.emse.fr
15-05-2016: Initial release
*/

#pragma once

/*!
* \file LayeredHashMapMathematics.h
* \brief Mathematic constants and functions for the HashMap.
* \author Matthieu Pinard
*/

// The function returns the integer part of log2(X) by retrieving the position of the most significant set bit.
// It uses bit twiddling hacks (as seen on graphics.stanford.edu) to favor speed.

const unsigned long b32[] = { 0x2, 0xC, 0xF0, 0xFF00, 0xFFFF0000 };
const unsigned long long b64[] = { 0x2, 0xC, 0xF0, 0xFF00, 0xFFFF0000, 0xFFFFFFFF00000000 };

const size_t * b = (sizeof(size_t) <= 4) ? (const size_t*)b32 : (const size_t*)b64;
const size_t S[] = { 1, 2, 4, 8, 16, 32 };
constexpr int Log2Iterations = (sizeof(size_t) <= 4) ? 4 : 5;

inline size_t IntLog2(size_t X) {
	size_t Result = 0U;
	for (auto i = Log2Iterations; i >= 0; i--) {
		if (X & b[i]) {
			X >>= S[i];
			Result |= S[i];
		}
	}
	return Result;
}

// Arrays of prime numbers P (starting at Primes[0] or __Primes[1]) and powers of two (NP), satisfying some conditions.
// i being the index :
// NP[i] < P[i + 1] < NP[i + 1]
// NP[i + 1] = 2 * NP[i] (consecutive powers of two)
// P[i + 1] > P[i] + NP[i]
// P[i] - NP[i] < P[0]

const unsigned long __Primes_32b[] = {
	0, 757, 1783, 3833, 7937,
	16141, 32537, 65327, 130873,
	261977, 524123, 1048433, 2097013,
	4194167, 8388473, 16777121, 33554341,
	67108777, 134217649, 268435399, 536870869,
	1073741789, 2147483629, 4294967291
};

const unsigned long long __Primes_64b[] = {
	0, 2633, 6733, 14929, 31321, 64091, 
	129643, 260723, 522883, 1047173, 2095759, 
	4192919, 8387231, 16775849, 33553103, 67107569, 
	134216461, 268434193, 536869651, 1073740571, 2147482417, 
	4294966099, 8589933397, 17179867997, 34359737227, 68719475599, 
	137438952341, 274877905823, 549755812831, 1099511626727, 2199023254517, 
	4398046510073, 8796093021181, 17592186043451, 35184372087881, 70368744176729, 
	140737488354413, 281474976709757, 562949953420457, 1125899906841811, 2251799813684467, 
	4503599627369863, 9007199254740397
};

const unsigned long NextPower_32b[] = {
	(1U << 10) - 1, (1U << 11) - 1, (1U << 12) - 1, (1U << 13) - 1,
	(1U << 14) - 1, (1U << 15) - 1, (1U << 16) - 1, (1U << 17) - 1,
	(1U << 18) - 1, (1U << 19) - 1, (1U << 20) - 1, (1U << 21) - 1,
	(1U << 22) - 1, (1U << 23) - 1, (1U << 24) - 1, (1U << 25) - 1,
	(1U << 26) - 1, (1U << 27) - 1, (1U << 28) - 1, (1U << 29) - 1,
	(1U << 30) - 1, (1U << 31) - 1, 4294967295
};

const unsigned long long NextPower_64b[] = {
	(1ULL << 12) - 1, (1ULL << 13) - 1,
	(1ULL << 14) - 1, (1ULL << 15) - 1, (1ULL << 16) - 1, (1ULL << 17) - 1,
	(1ULL << 18) - 1, (1ULL << 19) - 1, (1ULL << 20) - 1, (1ULL << 21) - 1,
	(1ULL << 22) - 1, (1ULL << 23) - 1, (1ULL << 24) - 1, (1ULL << 25) - 1,
	(1ULL << 26) - 1, (1ULL << 27) - 1, (1ULL << 28) - 1, (1ULL << 29) - 1,
	(1ULL << 30) - 1, (1ULL << 31) - 1, (1ULL << 32) - 1, (1ULL << 33) - 1,
	(1ULL << 34) - 1, (1ULL << 35) - 1, (1ULL << 36) - 1, (1ULL << 37) - 1,
	(1ULL << 38) - 1, (1ULL << 39) - 1, (1ULL << 40) - 1, (1ULL << 41) - 1,
	(1ULL << 42) - 1, (1ULL << 43) - 1, (1ULL << 44) - 1, (1ULL << 45) - 1,
	(1ULL << 46) - 1, (1ULL << 47) - 1, (1ULL << 48) - 1, (1ULL << 49) - 1,
	(1ULL << 50) - 1, (1ULL << 51) - 1, (1ULL << 52) - 1, (1ULL << 53) - 1
};

const size_t * NextPower = (sizeof(size_t) <= 4) ? (const size_t*)NextPower_32b : (const size_t*)NextPower_64b;

// So that Primes[-1] == __Primes[0] = 0. 
// This is done to improve performance as it prevents a conditional. 

const size_t * Primes = (sizeof(size_t) <= 4) ? (const size_t*)&__Primes_32b[1] : (const size_t*)&__Primes_64b[1];

// Equals log2(NextPower[0] + 1) - 1

constexpr size_t LowestExponent = (sizeof(size_t) <= 4) ? 9 : 11;

// Equals countof(Primes)

constexpr size_t MaxLayerCount = (sizeof(size_t) <= 4) ? 23 : 42;

//Equals 2^LowestExponent

constexpr size_t LowestNextPower = (sizeof(size_t) <= 4) ? 512 : 2048;
