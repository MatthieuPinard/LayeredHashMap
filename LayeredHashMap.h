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

// to do :                            
// AllocateLayer() in MT context    
// Move & Copy CTORs
// Safe iterator() 

/*!
* \file LayeredHashMap.h
* \brief A concurrency-safe HashMap implemented in C++11.
* \author Matthieu Pinard
*/
#include "LayeredHashMapMathematics.h"
#include "LayeredHash.h"
#include "AtomicRWLock.h"
#include "ThreadManager\ThreadManager.h"
#include <memory>
#include <array>
#include <functional>
#include <list>

#define MAX_INSTANCE_COUNT 1024

template<class T>
class InitalizedVector {
private:
	std::vector<T> Data;
public:
	template<class U>
	explicit InitalizedVector(U* pU) {
		Data.reserve(MAX_INSTANCE_COUNT);
		for (auto i = 0U; i < MAX_INSTANCE_COUNT; ++i) {
			Data.emplace_back(*(pU + i));
		}
	}
	T& operator[] (size_t Idx) {
		return Data[Idx];
	}
};

template<class T>
class ConcurrentList {
private:
	std::list<T> Data;
	AtomicLock Lock;
public:
	ConcurrentList() {
		for (auto i = 0U; i < MAX_INSTANCE_COUNT; ++i) {
			Data.push_front(i);
		}
	}
	inline void push_front (const T& Val) {
		std::lock_guard<AtomicLock> lock(Lock);
		Data.push_front(Val);
	}
	inline void push_front(T&& Val) {
		std::lock_guard<AtomicLock> lock(Lock);
		Data.push_front(Val);
	}
	inline T pop_front() {
		std::lock_guard<AtomicLock> lock(Lock);
		T Ret = Data.front();
		Data.pop_front();
		return Ret;
	}
};

// Allocate a static list of values {0 ... MAX_INSTANCE_COUNT - 1} so LayeredHashMap constructor can pick out an index in the 
// to bind to a free ThreadManager/ThreadValues... This is concurrency-safe.
static ConcurrentList<size_t> AvailableInstanceIdx;

// Allocate a static array of ThreadManager of size MAX_INSTANCE_COUNT, so multiple instances of the LayeredHashMap can be used at once.
static std::array<ThreadManager, MAX_INSTANCE_COUNT> Managers;

// Allocate a vector of ThreadValues per thread using the thread_local keyword.
// The number of ThreadValues per thread is equal to MAX_INSTANCE_COUNT, and they are initialized so that
// the i-th ThreadValue is bound to the i-th ThreadManager.
static thread_local InitalizedVector<ThreadValue> Values(&Managers[0]);

/*! \class LayeredHashMap
* \brief Class implementing the HashMap.
*
*  The class provides with Read, Write, Delete, and Size retrieval capabilities.
*/
template <class K, class T, class Hash = LayeredHash<K>, class Pred = std::equal_to<K>, class Alloc = std::allocator<K> >
class LayeredHashMap
{
	// Allocator typedef to rebind Alloc to other types
	template<typename __T>
	using Allocator = typename Alloc::template rebind<__T>::other;
	// 1-D vector
	template<typename __T>
	using Vector = std::vector<__T, Allocator<__T> >;
	// 2-D vector = array of vector
	template<typename __T>
	using ArrayVector = std::array<Vector<__T>, MaxLayerCount>;
	// Pair definition
	typedef std::pair<K, T> Pair;
	// Slot definition
	struct Slot {
		AtomicRWLock Lock; // Read-Write Lock
		Pair Main;  // Main KeyValue
		Vector<Pair> Collisions; // Collided KeyValues
	};
private:
	ArrayVector<Slot> Slots; /*!< A ArrayVector (ie. bidimensional) containing the Slots */
	const size_t InstanceIdx;  /*!< The variable containing the index of this HashMap instance. (0 to MAX_INSTANCE_COUNT - 1) */
	size_t LayerLastIdx; /*!< The variable containing the last used Vector index in the HashMap */
	// Defines a lambda function which is used by the ThreadManager to resize the table when needed.
	#define RESIZE_FUNC				[=](uInt GlobalValue) -> uInt {						\
										if (GlobalValue > Primes[LayerLastIdx]) {		\
											AllocateLayer();							\
										}												\
										return Primes[LayerLastIdx];					\
									}
	// Resize the Slot container within a single macro.
	#define MAP_ALLOC(Idx, Size)	{  Slots[Idx].resize(Size);	 }
	// Dest is updated with Src.back(), and the last element of Src is deleted.		
	#define SWAP_AND_POP(Dest, Src) {  Dest = std::move(Src.back());					\
									   Src.pop_back(); }
	// Initialize the class' fields within a single macro.
	#define MAP_INIT()				{  Managers[InstanceIdx].SetCallback(RESIZE_FUNC);	\
									   auto FirstPrime = Primes[0U];					\
									   MAP_ALLOC(0U, FirstPrime); }
private:
	/*!
	*  \brief Computes the raw hash of a Key.
	*/
	inline size_t RawHash(K const&) const;
	/*!
	*  \brief Computes the Layer index of a Key, given its raw hash passed as argument.
	*/
	inline size_t GetLayer(size_t const) const;
	/*!
	*  \brief Computes the Slot index (in the Layer) of a Key, given its raw hash and its Layer index, passed as argument.
	*/
	inline size_t GetSlot(size_t const, size_t const) const;
public:
	/*!
	*  \brief Allocate a new Layer in the LayeredHashMap.
	*
	*  This method allocates a new Layer, and moves the currently stored elements to their new position.
	*/
	void AllocateLayer();
public:
	/*!
	*  \brief Returns the LayeredHashMap size.
	*
	*  This method uses a ThreadManager to synchronize the sizes stored within each thread of execution.
	*
	*  \return The number of elements stored into the HashMap.
	*/
	inline size_t GetSize();
	/*!
	*  \brief Write the Key and Value passed as argument inside the LayeredHashMap.
	*
	*  \param Key The Key to be inserted.
	*
	*  \param Val The Value to be inserted.
	*
	*  This method is thread-safe as it locks the Slot before writing to it.
	*
	*/
	void Write(K const& Key, T const& Val);
	/*!
	*  \brief Delete the Key passed as argument from the LayeredHashMap.
	*
	*  This method is thread-safe as it locks the Slot before deleting.
	*
	*  \param Key The Key to be deleted.
	*
	*  \return true if the function has deleted the Key,
	*  false otherwise. 
	*/
	bool Delete(K const& Key);
	/*!
	*  \brief Read the Value whose Key is the one passed as argument.
	*
	*  This method throws std::out_of_range if the Key passed as argument is not found in the LayeredHashMap.
	*  \param Key The Key which corresponding Value has to be found.
	*
	*  \return The Value whose Key is the one passed as argument.
	*/
	T Read(K const& Key); 
	/*!
	*  \brief LayeredHashMap constructor.
	*
	*  This method allocates the first Layer.
	*/
	LayeredHashMap() : LayerLastIdx(0U), InstanceIdx(AvailableInstanceIdx.pop_front()) {
		MAP_INIT();
	}
	/*!*
	*  \brief LayeredHashMap constructor with initial size hint.
	*
	*  \param InitialSize The desired initial size.
	*
	*  This method allocates Layers, so the initial size of the LayeredHashMap is greater or equal to InitialSize.
	*/
	LayeredHashMap(const size_t InitialSize) : LayerLastIdx(0U), InstanceIdx(AvailableInstanceIdx.pop_front()) {
		MAP_INIT();
		while (Primes[LayerLastIdx] < InitialSize) {
			AllocateLayer();
		}
	}
	/*!
	*  \brief LayeredHashMap destructor.
	*
	*  This method appends the instance index into the available instance index list, so it can be reused afterwards.
	*/
	~LayeredHashMap() {
		Managers[InstanceIdx].Reset();
		AvailableInstanceIdx.push_front(InstanceIdx);
	}
};

template <class K, class T, class Hash, class Pred, class Alloc>
void LayeredHashMap<K, T, Hash, Pred, Alloc>::AllocateLayer() {
	// Allocate a new Vector in the Hashtable.
	auto OldPrime = Primes[LayerLastIdx++];
	auto NewPrime = Primes[LayerLastIdx];
	auto DeltaPrime = NewPrime - OldPrime;
	MAP_ALLOC(LayerLastIdx, DeltaPrime);
	// Move elements
	/* To do ... */
}

template <class K, class T, class Hash, class Pred, class Alloc>
inline size_t LayeredHashMap<K, T, Hash, Pred, Alloc>::GetSize() {
	return Managers[InstanceIdx].GetGlobalValue();
}

template <class K, class T, class Hash, class Pred, class Alloc>
inline size_t LayeredHashMap<K, T, Hash, Pred, Alloc>::RawHash(K const& Key) const {
	return (Hash()(Key) & NextPower[LayerLastIdx]) % Primes[LayerLastIdx];
}

template <class K, class T, class Hash, class Pred, class Alloc>
inline size_t LayeredHashMap<K, T, Hash, Pred, Alloc>::GetLayer(size_t const rawHash) const {
	// If rawHash is < LowestNextPower, we add LowestNextPower to it and compute the Log2.
	// But Log2(Sum) < Log2(2*LowestNextPower) so Log2(Sum) = Log2(LowestNextPower) = LowestExponent.
	// So LayerIdx = 0U in this case.
	// If rawHash >= LowestNextPower it simply is Log2(rawHash) - LowestExponent >= 0
	auto LayerIdx = IntLog2(rawHash + (rawHash < LowestNextPower) * LowestNextPower) - LowestExponent;
	// If rawHash exceeds Prime[LayerIdx] we just take the next Vector.
	return LayerIdx + (rawHash >= Primes[LayerIdx]);
}

template <class K, class T, class Hash, class Pred, class Alloc>
inline size_t LayeredHashMap<K, T, Hash, Pred, Alloc>::GetSlot(size_t const rawHash, size_t const LayerIdx) const {
	return rawHash - Primes[LayerIdx - 1];
}

template <class K, class T, class Hash, class Pred, class Alloc>
void LayeredHashMap<K, T, Hash, Pred, Alloc>::Write(K const& Key, T const& Value) {
	auto rawHash = RawHash(Key);
	auto LayerIdx = GetLayer(rawHash);
	auto SlotIdx = GetSlot(rawHash, LayerIdx);
	auto& MainPair = Slots[LayerIdx][SlotIdx].Main;
	WriteWrapper WriteLock(Slots[LayerIdx][SlotIdx].Lock);
	// If the slot is empty, simply write the new KeyVal in the Main KeyVal, and increment the Size.
	if (WriteLock() == EMPTY) {
		MainPair = std::move(Pair(Key, Value));
		Values[InstanceIdx].Increment();
	}
	// If the Main Key is already correct, simply replace the Value.
	else if (Pred()(MainPair.first, Key)) {
		MainPair.second = Value;
	}
	// Check for Collisions.
	else {
		auto CollisionIt = std::move(std::find_if(Slots[LayerIdx][SlotIdx].Collisions.begin(), 
												  Slots[LayerIdx][SlotIdx].Collisions.end(), 
												  [&](Pair const& _KeyVal) -> bool {
			return Pred()(_KeyVal.first, Key);
		}));
		// If a collision is not found, append the new KeyVal at the end of the collisions vector and increment the Size.
		if (CollisionIt == Slots[LayerIdx][SlotIdx].Collisions.end()) {
			Slots[LayerIdx][SlotIdx].Collisions.emplace_back(Key, Value);
			Values[InstanceIdx].Increment();
		}
		// Otherwise, update the value of the current collided Value.
		else {
			(*CollisionIt).second = Value;
		}
	}
	WriteLock() = POPULATED;
}

template <class K, class T, class Hash, class Pred, class Alloc>
bool LayeredHashMap<K, T, Hash, Pred, Alloc>::Delete(K const& Key) {
	auto rawHash = RawHash(Key);
	auto LayerIdx = GetLayer(rawHash);
	auto SlotIdx = GetSlot(rawHash, LayerIdx);
	auto deletionOccured = true;
	WriteWrapper WriteLock(Slots[LayerIdx][SlotIdx].Lock);
	auto NewSlotStatus = WriteLock();
	// Empty slot : nothing to delete.
	if (NewSlotStatus == EMPTY) {
		deletionOccured = false; 
	} 
	// The key is found in the Main value:
	else if (Pred()(Slots[LayerIdx][SlotIdx].Main.first, Key)) {
		// Take the last collision and make it the new Main value, so the current Main value is erased.
		if (!Slots[LayerIdx][SlotIdx].Collisions.empty()) {
			SWAP_AND_POP(Slots[LayerIdx][SlotIdx].Main, Slots[LayerIdx][SlotIdx].Collisions);
		}
		// If there are no collisions, the slot is empty.
		else {
			NewSlotStatus = EMPTY;
		}
	}
	// Traverse the Collision vector.
	else {
		auto CollisionIt = std::move(std::find_if(Slots[LayerIdx][SlotIdx].Collisions.begin(), 
												  Slots[LayerIdx][SlotIdx].Collisions.end(), 
												  [&](Pair const& _KeyVal) -> bool {
			return Pred()(_KeyVal.first, Key);
		}));
		// Take the last collision and move it to the found value (which will be deleted).
		if (CollisionIt != Slots[LayerIdx][SlotIdx].Collisions.end()) {
			SWAP_AND_POP(*CollisionIt, Slots[LayerIdx][SlotIdx].Collisions);
		}
		// Key not found : return false.
		else {
			deletionOccured = false;
		}
	}
	// Decrement the size if a deletion occured.
	if (deletionOccured) {
		Values[InstanceIdx].Decrement();
	}
	WriteLock() = NewSlotStatus;
	return deletionOccured;
}

template <class K, class T, class Hash, class Pred, class Alloc>
T LayeredHashMap<K, T, Hash, Pred, Alloc>::Read(K const& Key) {
	auto rawHash = RawHash(Key);
	auto LayerIdx = GetLayer(rawHash);
	auto SlotIdx = GetSlot(rawHash, LayerIdx);
	ReadWrapper ReadLock(Slots[LayerIdx][SlotIdx].Lock);
	// Empty slot : throw an exception.
	if (ReadLock() == EMPTY) {
		throw std::out_of_range("The key was not found in the LayeredHashMap structure: The Slot was not populated.");
	}
	// Look in the main value for equal keys.
	if (Pred()(Slots[LayerIdx][SlotIdx].Main.first, Key)) {
		return Slots[LayerIdx][SlotIdx].Main.second;
	}
	// Look in the collision vector for equal keys.
	auto CollisionIt = std::move(std::find_if(Slots[LayerIdx][SlotIdx].Collisions.begin(),
											  Slots[LayerIdx][SlotIdx].Collisions.end(), 
											  [&](Pair const& _KeyVal) -> bool {
		return Pred()(_KeyVal.first, Key);
	}));
	// If it is not found, throw an exception.
	if (CollisionIt == Slots[LayerIdx][SlotIdx].Collisions.end()) {
		throw std::out_of_range("The key was not found in the LayeredHashMap structure: The Key was not found in the Slot.");
	}
	return CollisionIt->second;
}
