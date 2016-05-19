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
* \file PlatformAtomic.h
* \brief Define Read/Write specialization for X86 and AMD64 platforms.
* \author Matthieu Pinard
*/

// sInt = signed type
// uInt = unsigned type
// uInt and sInt have the same size, the size of size_t.
#if (defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_AMD64)))
	#if defined(_M_IX86) 
		typedef long sInt;
		typedef unsigned long uInt;
		#define ALIGNED __declspec(align(4)) // Intel 8.1.1: 32-bits RW on 32-bit aligned memory are atomic.
	#elif defined(_M_AMD64)
		typedef long long sInt;
		typedef unsigned long long uInt;
		#define ALIGNED __declspec(align(8)) // Intel 8.1.1: 64-bits RW on 64-bit aligned memory are atomic.
	#endif
	typedef sInt _sInt;
	#define ATOMIC_WRITE(X, Val) (X = (Val))
	#define ATOMIC_READ(X) X
	#define INCREMENT(X) (++X)
	#define DECREMENT(X) (--X)
	#define VOLATILE volatile // Microsoft specific: Volatile reads have acquire semantics, volatile writes have release semantics.
#else
#include <atomic>
	typedef std::atomic<ptrdiff_t> sInt;
	typedef ptrdiff_t _sInt;
	typedef size_t uInt;
	#define ATOMIC_READ(X) ((X).load(std::memory_order::memory_order_acquire))
	#define ATOMIC_WRITE(X, Val) ((X).store(Val, std::memory_order::memory_order_release))
	#define INCREMENT(X) ((X).fetch_add(1, std::memory_order::memory_order_release))
	#define DECREMENT(X) ((X).fetch_add(-1, std::memory_order::memory_order_release))
	#define ALIGNED
	#define VOLATILE
#endif