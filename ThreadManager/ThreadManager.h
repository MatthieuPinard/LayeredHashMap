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
* \file ThreadManager.h
* \brief Managing C++11 thread_local storage from a single class
* \author Matthieu Pinard
*/
#include <functional>
#include <vector>
#include <algorithm>
#include <mutex>
#include "AtomicLock.h"
#include "PlatformAtomic.h"

const double MAX_ERROR = 0.00001; /* 0.001 error rate % */

/*! \class ThreadValue
* \brief Thread-local storage class.
*
*  The class provides with Increment, Decrement methods to modify its Value,
*  as well as field getters/setters to be accessed from the ThreadManager class.
*/
class ThreadValue;
/*! \class ThreadManager
* \brief Class used to manage ThreadValues.
*/
class ThreadManager;

class ThreadManager {
public://private:
	_sInt DTOR_ThreadValuesSum; /*!< The signed integer storing the sum of deleted ThreadValues */
	std::vector<ThreadValue*> ThreadValues; /*!< The vector containing pointers to ThreadValue objects */
	AtomicLock ManagerLock; /*!< The Lock used to complete Thread-safe operations on the structure */
	AtomicLock ValueLock;
	std::unique_lock<AtomicLock> ValueUniqueLock; /*!< The Lock used to retrieve the exact sum of ThreadValues, as it blocks subsequent Increments and Decrements */
	std::function<uInt(uInt)> Callback; /*!< The user-defined Callback called each time the ThreadManager is updated.
										It takes the global value (= sum of ThreadValues) as argument and returns a "goal" global value for the next update. */
	/*!
	*  \brief Update the ThreadManager class from a Single-Thread context. 
	*  The public available function is UpdateManager(), which is MT-safe.
	*
	*  This method computes the global value (= sum of ThreadValues), calls the Callback function to retrieve the new "goal" global value from the current global value,
	*  and sets new thresholds for ThreadValues, so that the next update will happen near the "goal" global value.
	*/
	void _UpdateManagerInternal();
	/*!
	*  \brief Block any subsequent calls to Increment() or Decrement() for any ThreadValue while the ValueUniqueLock is hold.
	*
	*/
	inline void WaitForGlobalValue() const;
public:
	/*!
	*  \brief ThreadManager constructor.
	*
	*  The contructor defines a "standard" Callback function for subsequent ThreadValue initializations, this Callback can still be modified by the SetCallback() method.
	*/
	ThreadManager() : DTOR_ThreadValuesSum(0), ValueUniqueLock(std::unique_lock<AtomicLock>(ValueLock, std::defer_lock)) {
		Callback = [](uInt) -> uInt {
			return Primes[0];
		};
	}
	/*!
	*  \brief Put the ThreadManager in its initial state.
	*
	* It does not re-initialize ThreadValues, as it would cause problems for ThreadValue DTOR.
	*/
	void Reset() {
		DTOR_ThreadValuesSum = 0;
		ValueUniqueLock = std::unique_lock<AtomicLock>(ValueLock, std::defer_lock);
		Callback = [](uInt) -> uInt {
			return Primes[0];
		};
	}    
	/*!
	*  \brief Sets the Callback function for the ThreadManager instance.
	*
	*/
	template<class Fn>
	void SetCallback(Fn&& fn);
	/*!
	*  \brief Remove the ThreadValue from the ThreadManager in a thread-safe way.
	*
	*  Deletes the ThreadValue given as argument from the ThreadValues vector, after adding its Value to the DTOR_ThreadValuesSum field.
	*/
	void DestructThreadValue(ThreadValue*); 
	/*!
	*  \brief Append the ThreadValue to the ThreadManager in a thread-safe way.
	*
	*  Appends the ThreadValue given as argument to the ThreadValues vector, and updates the ThreadManager, as a higher thread count 
	*  implies smaller thresholds for the ThreadValues.
	*/
	void ConstructThreadValue(ThreadValue*);
	/*!
	*  \brief Update the ThreadManager in a thread-safe way.
	*
	*  This function is coded so that only one thread can update the ThreadManager at a given time.
	*  If another ThreadValue calls this function, that means that it has exceeded its Threshold. Thus, it waits for the
	*  first thread to return from the function, blocking calls to Increment or Decrement in the meanwhile, then returns.
	*/
	inline void UpdateManager();
	/*!
	*  \brief Retrieve the so-called "global value" (= sum of ThreadValues) in a thread-safe way.
	*
	*  Holds ValueUniqueLock so no subsequent calls to Increment() or Decrement() can occur for any ThreadValue,
	*  thus enabling the ability to retrieve the exact global value.
	* 
	*  \return The sum of ThreadValues, including deleted ones, at the time the function is called.
	*/
	inline uInt GetGlobalValue();
};

class ThreadValue {
private:
	ALIGNED VOLATILE sInt Value; /*!< Field storing the value, as a signed integer */
	ALIGNED VOLATILE sInt Threshold; /*!< Threshold used for Increment(), so that it will try to update the ThreadManager if Value exceeds the Threshold */
	ThreadManager& Manager; /*!< Reference to a ThreadManager */
public:
	/*!
	*  \brief ThreadValue constructor.
	*  \param A reference to a ThreadManager object.
	*
	*  Calls ThreadManager::ConstructThreadValue(this).
	*/
	ThreadValue(ThreadManager&);
	/*!
	*  \brief ThreadValue destructor.
	*
	*  Calls ThreadManager::DestructThreadValue(this).
	*/
	~ThreadValue();
	/*!
	*  \brief Increment the Value.
	*
	*  This function tries to update the ThreadManager if the Value exceeds the Threshold.
	*  It also waits for the ThreadManager::ValueUniqueLock to be released (see ThreadManager::GetGlobalValue())
	*/
	inline void Increment();
	/*!
	*  \brief Decrement the Value.
	*
	*  It also waits for the ThreadManager::ValueUniqueLock to be released (see ThreadManager::GetGlobalValue())
	*/
	inline void Decrement();
	/*!
	*  \brief Atomically replaces Threshold by (Value + X).
	*  \param A signed integer X
	*/
	inline void AdjustThreadThreshold(_sInt);
	/*!
	*  \brief Atomically retrieves the Value.
	*/
	inline _sInt GetThreadValue() const;
};

inline _sInt ThreadValue::GetThreadValue() const {
	return ATOMIC_READ(Value);
}

inline void ThreadValue::AdjustThreadThreshold(_sInt Adjustment) {
	ATOMIC_WRITE(Threshold, ATOMIC_READ(Value) + Adjustment);
}

inline void ThreadValue::Increment() {
	// Try to update when the Threshold is exceeded.
	if (INCREMENT(Value) >= ATOMIC_READ(Threshold)) {
		Manager.UpdateManager();
	}
	// Used by GetGlobalValue() to retrieve the exact Global Value. 
	Manager.WaitForGlobalValue();
}

inline void ThreadValue::Decrement() {
	DECREMENT(Value);
	// Used by GetGlobalValue() to retrieve the exact Global Value. 
	Manager.WaitForGlobalValue();
}

ThreadValue::ThreadValue(ThreadManager& _Manager) : Value(0), Manager(_Manager) {
	Manager.ConstructThreadValue(this);
}

ThreadValue::~ThreadValue() {
	Manager.DestructThreadValue(this);
}

template<class Fn>
void ThreadManager::SetCallback(Fn && fn) {
	Callback = fn;
}

void ThreadManager::DestructThreadValue(ThreadValue* pThreadValue) {
	ManagerLock.lock();
	// Find the correct ThreadValue.
	auto itThreadValue = std::move(std::find_if(ThreadValues.begin(), ThreadValues.end(), [pThreadValue](ThreadValue* _p) -> bool {
		return (_p == pThreadValue);
	}));
	// Atomically retrieve its value and delete it from the ThreadValues vector.
	DTOR_ThreadValuesSum += (*itThreadValue)->GetThreadValue();
	std::swap(*itThreadValue, ThreadValues.back());
	ThreadValues.pop_back();
	ManagerLock.unlock();
}

void ThreadManager::_UpdateManagerInternal() {
	// Compute the Global Value.
	_sInt ThreadValuesSum = 0;
	std::for_each(ThreadValues.begin(), ThreadValues.end(), [&](ThreadValue *& ThrValue) {
		ThreadValuesSum += ThrValue->GetThreadValue();
	});
	auto GlobalValue = uInt(ThreadValuesSum + DTOR_ThreadValuesSum);
	// The Callback() function is in charge of computing the new Threshold based on the Global Value.
	auto Threshold = Callback(GlobalValue);
	auto NewMargin = std::max(_sInt(Threshold - GlobalValue),
		// Optimal margin when the work is properly balanced within threads.
		_sInt(Threshold * MAX_ERROR))
		// However it tends to converge quickly, so we impose a minimal change within Updates.
		/ uInt(ThreadValues.size());
	// Adjust ThreadValues thresholds.
	std::for_each(ThreadValues.begin(), ThreadValues.end(), [NewMargin](ThreadValue *& ThrValue) {
		ThrValue->AdjustThreadThreshold(NewMargin);
	});
}

void ThreadManager::ConstructThreadValue(ThreadValue* pThreadValue) {
	ManagerLock.lock();
	// Insert the new ThreadValue and update the manager 
	// (as the thread count increases, the threshold must decrease so we don't exceed the set "goal" Global Value) 
	ThreadValues.push_back(pThreadValue);
	_UpdateManagerInternal();
	ManagerLock.unlock();
}

inline void ThreadManager::UpdateManager() {
	// If a thread is already updating, don't update, but rather wait.
	// This is done because if a thread is calling UpdateManager(), that means
	// it has exceeded its Threshold, so don't do any further Increment/Decrement until a Threshold is computed.
	if (!ManagerLock.try_lock()) {
		ManagerLock.wait();
	}
	else {
		// No thread is updating, so acquire the Lock and update it.
		_UpdateManagerInternal();
		ManagerLock.unlock();
	}
}

inline uInt ThreadManager::GetGlobalValue() {
	// The ValueUniqueLock is used so no further calls to Increment/Decrement can be issued, thus
	// allowing for exact retrieval of the sum of ThreadValues::Value.
	ValueUniqueLock.lock();
	ManagerLock.lock();
	// Compute the sum. 
	_sInt ThreadValuesSum = 0;
	std::for_each(ThreadValues.begin(), ThreadValues.end(), [&](ThreadValue *& ThrValue) {
		ThreadValuesSum += ThrValue->GetThreadValue();
	});
	ManagerLock.unlock();
	ValueUniqueLock.unlock();
	return uInt(ThreadValuesSum + DTOR_ThreadValuesSum);
}

inline void ThreadManager::WaitForGlobalValue() const {
	while (ValueUniqueLock.owns_lock());
}