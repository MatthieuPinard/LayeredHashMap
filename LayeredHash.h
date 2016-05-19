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
* \file LayeredHash.h
* \brief Define Hash function specialization for std::basic_string, std::pair, pointers and types castable to size_t.
* \author Matthieu Pinard
*/

/*! \class LayeredHash
* \brief The Hasher class used in LayeredHashMap.
*/
template <typename __T>
class LayeredHash {
public:
	inline size_t operator() (__T const& Key) const {
		return _Hash(Key);
	}
};

// Castable to size_t.
template <typename __T>
inline size_t _Hash(__T const& Key) {
	return static_cast<size_t>(Key);
}

// Pointers.
template <typename __T>
inline size_t _Hash(__T* Key) {
	return reinterpret_cast<size_t>(ptr);
}

// Basic_string
template<typename charT, typename traits, typename Alloc>
inline size_t _Hash(std::basic_string<charT, traits, Alloc> const& String) {
	const charT* Str = String.data();
	size_t Count = String.length();
	size_t Hash = 5381;
	for (size_t i = 0; i < Count; ++i) {
		Hash += Str[i];
		Hash = (Hash << 5) + Hash;
	}
	return Hash;
}

// std::pair
template <typename __X, typename __Y>
inline size_t _Hash(std::pair<__X, __Y> const& Pair) {
	return _Hash(Pair.first) ^ _Hash(Pair.second);
}