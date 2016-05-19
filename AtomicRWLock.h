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
* \file AtomicRWLock.h
* \brief Thread Synchronization capabilities using C++11 <atomic>
* \author Matthieu Pinard
*/
#include <atomic>
#include <thread>

// Constants used for the Lock. The uint_fast32_t is standard (as of C99), can be used with std::atomic<> and at least 32 bits.
const uint_fast32_t EMPTY = 0x00000000, POPULATED = 0x80000000;
const uint_fast32_t VALUE_BITS_MASK = 0x80000000, WRITER_BIT_MASK = 0x40000000, READER_COUNT_MASK = 0x3FFFFFFF;

/*! \class AtomicLock
* \brief Class for locking objects, working like a Read-Write Lock (allowing multiple Readers but a single Writer, at the cost of a certain overhead).
*		 It is then suited well for a large number of readers compared to writers.
*
*  The class provides with lock, unlock capabilities for both Read and Write operations.
*/
class AtomicRWLock {
private:
	std::atomic<uint_fast32_t> ThisLock; /*!< The atomic variable containing the Lock state, including its value (EMPTY or POPULATED) in VALUE_BITS, whether the Lock is acquired for Writing or not
										 (WRITER_BIT) and the spin count (READER_COUNT)
										 0	 2	 3		     31
										 |-------|-------|-------------------|
										 |VALUE	 |WRITER |READER_COUNT       |
										 |BITS	 |BIT	 |		     |
										 |		 |		     |					 |
										 |-------|-------|-------------------|
										 */
public:
	/*!
	*  \brief Acquire the AtomicLock for writing, so no other subsequent Read/Write operations can occur, and return the Lock stored VALUE_BITS.
	*
	*  This methods spins while the AtomicLock is acquired for writing by another thread.
	*  As soon as it is released, the write_lock() function tries to acquire the Lock for writing, and wait for potential Readers before returning.
	*
	*/
	inline uint_fast32_t write_lock();
	/*!
	*  \brief Release the AtomicLock, and store the value passed as argument in the VALUE_BITS.
	*/
	inline void write_unlock(const uint_fast32_t);
	/*!
	*  \brief Acquire the AtomicLock for reading, so no other subsequent Write operations can occur, and return the Lock stored VALUE_BITS.
	*
	*  This methods spins while the AtomicLock is acquired for writing by another thread.
	*  As soon as it is released, the read_lock() function increments the READER_COUNT.
	*
	*/
	inline uint_fast32_t read_lock();
	/*!
	*  \brief Decrements the READER_COUNT.
	*/
	inline void read_unlock();
	/*!
	*  \brief AtomicLock constructors.
	*
	*  The contructor initializes the AtomicLock as empty and released for Writing and Reading.
	*/
	AtomicRWLock() : ThisLock(EMPTY) {}
	AtomicRWLock(const AtomicRWLock &) : ThisLock(EMPTY) {}
	AtomicRWLock(AtomicRWLock &&) : ThisLock(EMPTY) {}
	/*!
	*  \brief AtomicLock destructor.
	*
	*/
	~AtomicRWLock() {}
};

inline uint_fast32_t AtomicRWLock::read_lock() {
	do {
		// Spin on atomic reading for speed.
		auto OldLock = ThisLock.load(std::memory_order_acquire);
		// If the Lock is available... (ie. no threads have locked it for writing)
		if (!(OldLock & WRITER_BIT_MASK)) {
			// Increment the READER_COUNT using a CAS-operation.
			if (ThisLock.compare_exchange_strong(OldLock, OldLock + 1, std::memory_order_acquire, std::memory_order_relaxed)) {
				// Return the VALUE_BITS.
				return OldLock & VALUE_BITS_MASK;
			}
		}
		// Yield between two increment tries.
		std::this_thread::yield();
	} while (true);
}

inline uint_fast32_t AtomicRWLock::write_lock()
{
	do {
		// Spin on atomic reading for speed.
		auto OldLock = ThisLock.load(std::memory_order_acquire);
		// If the Lock is available... (ie. no threads have locked it for writing)
		if (!(OldLock & WRITER_BIT_MASK)) {
			// Set the WRITER_BIT using a CAS-operation.
			auto NewLock = OldLock | WRITER_BIT_MASK;
			if (ThisLock.compare_exchange_strong(OldLock, NewLock, std::memory_order_acquire, std::memory_order_relaxed)) {
				// Wait for the Readers before acquiring the Lock for writing.
				while (ThisLock.load(std::memory_order_acquire) & READER_COUNT_MASK)
					std::this_thread::yield();
				// Return the VALUE_BITS.
				return OldLock & VALUE_BITS_MASK;
			}
		}
		// Yield between two checks.
		std::this_thread::yield();
	} while (true);
}

inline void AtomicRWLock::write_unlock(const uint_fast32_t X) {
	// Set the VALUE_BITS (whether the Slot is POPULATED or EMPTY)
	ThisLock.store(X, std::memory_order_release);
}

inline void AtomicRWLock::read_unlock() {
	// Substract 1 from READER_COUNT
	ThisLock.fetch_sub(1, std::memory_order_release);
}

/*! \class ReadWrapper
* \brief RAII class for Read operations on an AtomicRWLock.
*
*/
class ReadWrapper {
private:
	uint_fast32_t Val; /*!< The Lock stored value */
	AtomicRWLock& Lck; /*!< The Lock as a reference */
public:
	/*!
	*  \brief ReadWrapper constructor.
	*
	*  The contructor locks the Lock passed as argument for Reading and stores the returned value.
	*/
	ReadWrapper(AtomicRWLock& _Lck) : Lck(_Lck) {
		Val = Lck.read_lock();
	}
	/*!
	*  \brief Getter/Setter for the Lock stored value.
	*/
	uint_fast32_t& operator() () {
		return Val;
	}
	/*!
	*  \brief ReadWrapper destructor.
	*
	*  The destructor unlocks the Lock passed as argument for Reading.
	*/
	~ReadWrapper() {
		Lck.read_unlock();
	}
};

/*! \class WriteWrapper
* \brief RAII class for Write operations on an AtomicRWLock.
*
*/
class WriteWrapper {
private:
	uint_fast32_t Val; /*!< The Lock stored value */
	AtomicRWLock& Lck; /*!< The Lock as a reference */
public:
	/*!
	*  \brief WriteWrapper constructor.
	*
	*  The contructor locks the Lock passed as argument for Writing and stores the returned value.
	*/
	WriteWrapper(AtomicRWLock& _Lck) : Lck(_Lck) {
		Val = Lck.write_lock();
	}
	/*!
	*  \brief Getter/Setter for the Lock stored value.
	*/
	uint_fast32_t& operator() () {
		return Val;
	}
	/*!
	*  \brief WriteWrapper destructor.
	*
	*  The destructor unlocks the Lock passed as argument for Writing.
	*/
	~WriteWrapper() {
		Lck.write_unlock(Val);
	}
};
