// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "irrlichttypes.h"
#include "debug.h" // sanity_check
#include "exceptions.h"
#include "threading/mutex_auto_lock.h"
#include "threading/semaphore.h"
#include <atomic>
#include <list>
#include <vector>
#include <map>
#include "../threading/lock.h"
#include "unordered_map_hash.h"
#include <unordered_set>
#include <queue>
#include <cassert>
#include <limits>

/*
	Queue with unique values with fast checking of value existence
*/

template<typename Value>
class UniqueQueue {
public:

	/*
	Does nothing if value is already queued.
	Return value:
	true: value added
	false: value already exists
	*/
	bool push_back(const Value& value)
	{
		if (m_set.insert(value).second)
		{
			m_queue.emplace(value);
			return true;
		}
		return false;
	}

	void pop_front()
	{
		m_set.erase(m_queue.front());
		m_queue.pop();
	}

	const Value& front() const
	{
		return m_queue.front();
	}

	u32 size() const
	{
		return m_queue.size();
	}

//private:
	std::unordered_set<Value, v3posHash, v3posEqual> m_set;
	std::queue<Value> m_queue;
};

/*
	Thread-safe map
*/

template<typename Key, typename Value>
class MutexedMap
{
public:
	MutexedMap() = default;

	void set(const Key &name, const Value &value)
	{
		MutexAutoLock lock(m_mutex);
		m_values[name] = value;
	}

	bool get(const Key &name, Value *result) const
	{
		MutexAutoLock lock(m_mutex);
		auto n = m_values.find(name);
		if (n == m_values.end())
			return false;
		if (result)
			*result = n->second;
		return true;
	}

	std::vector<Value> getValues() const
	{
		MutexAutoLock lock(m_mutex);
		std::vector<Value> result;
		result.reserve(m_values.size());
		for (auto it = m_values.begin(); it != m_values.end(); ++it)
			result.push_back(it->second);
		return result;
	}

	void clear() { m_values.clear(); }

private:
	std::map<Key, Value> m_values;
	mutable std::mutex m_mutex;
};


/*
Is fast at:
- Returning value by id (very fast)
- Returning id by value
- Generating a new id for a value

Is not able to:
- Remove an id/value pair (is possible to implement but slow)
*/
template<typename T>
class MutexedIdGenerator
{
public:
	MutexedIdGenerator()
	{
	}

	// Returns true if found
	bool getValue(u32 id, T &value)
	{
		if(id == 0)
			return false;
		MutexAutoLock lock(m_mutex);
		if(m_id_to_value.size() < id)
			return false;
		value = m_id_to_value[id-1];
		return true;
	}

	// If id exists for value, returns the id.
	// Otherwise generates an id for the value.
	u32 getId(const T &value)
	{
		MutexAutoLock lock(m_mutex);
		typename std::map<T, u32>::iterator n;
		n = m_value_to_id.find(value);
		if(n != m_value_to_id.end())
			return n->second;
		m_id_to_value.push_back(value);
		u32 new_id = m_id_to_value.size();
		m_value_to_id.insert(value, new_id);
		return new_id;
	}

private:
	std::mutex m_mutex;
	// Values are stored here at id-1 position (id 1 = [0])
	std::vector<T> m_id_to_value;
	std::map<T, u32> m_value_to_id;
};

/*
FIFO queue (well, actually a FILO also)
*/
template<typename T>
class Queue //TODO! rename me to shared_queue
: public shared_locker, public std::queue<T>
{
public:
	Queue() { }

	void push_back(T t)
	{
		const auto lock = lock_unique();
		std::queue<T>::push(t);
	}

	void push(T t)
	{
		const auto lock = lock_unique();
		std::queue<T>::push(t);
	}

	template<typename... Args>
	decltype(auto) emplace(Args&&... args)
	{
		const auto lock = lock_unique();
		return std::queue<T>::emplace(std::forward<Args>(args)...);
	}

	template<typename... Args>
	decltype(auto) emplace_back(Args&&... args)
	{
		const auto lock = lock_unique();
		return std::queue<T>::emplace_back(std::forward<Args>(args)...);
	}

	// usually used as pop_front()
	T front() = delete;
	void pop() = delete;

	T pop_front()
	{
		const auto lock = lock_unique();
		T val = std::queue<T>::front();
		std::queue<T>::pop();
		return val;
	}

	u32 size() const
	{
		const auto lock = lock_shared();
		return std::queue<T>::size();
	}

	bool empty() const
	{
		const auto lock = lock_shared();
		return std::queue<T>::empty();
	}
};

/*
	Thread-safe double-ended queue
*/

template<typename T>
class MutexedQueue
{
public:
	template<typename Key, typename U, typename Caller, typename CallerData>
	friend class RequestQueue;

	MutexedQueue() = default;

	bool empty() const
	{
		MutexAutoLock lock(m_mutex);
		return m_queue.empty();
	}
	bool empty_try()
	{
		MutexAutoLock lock(m_mutex);
		return m_queue.empty();
	}
	size_t size() {
		MutexAutoLock lock(m_mutex);
		return m_queue.size();
	}

	void push_back(const T &t)
	{
		MutexAutoLock lock(m_mutex);
		m_queue.push_back(t);
		m_signal.post();
	}

	void push_back(T &&t)
	{
		MutexAutoLock lock(m_mutex);
		m_queue.push_back(std::move(t));
		m_signal.post();
	}

	/* this version of pop_front returns an empty element of T on timeout.
	* Make sure default constructor of T creates a recognizable "empty" element
	*/
	T pop_frontNoEx(u32 wait_time_max_ms)
	{
		if (m_signal.wait(wait_time_max_ms)) {
			MutexAutoLock lock(m_mutex);

			T t = std::move(m_queue.front());
			m_queue.pop_front();
			return t;
		}

		return T();
	}

	T pop_front(u32 wait_time_max_ms)
	{
		if (m_signal.wait(wait_time_max_ms)) {
			MutexAutoLock lock(m_mutex);

			T t = std::move(m_queue.front());
			m_queue.pop_front();
			return t;
		}

		throw ItemNotFoundException("MutexedQueue: queue is empty");
	}

	T pop_frontNoEx()
	{
		m_signal.wait();

		MutexAutoLock lock(m_mutex);

		T t = std::move(m_queue.front());
		m_queue.pop_front();
		return t;
	}

	T pop_back(u32 wait_time_max_ms=0)
	{
		if (m_signal.wait(wait_time_max_ms)) {
			MutexAutoLock lock(m_mutex);

			T t = std::move(m_queue.back());
			m_queue.pop_back();
			return t;
		}

		throw ItemNotFoundException("MutexedQueue: queue is empty");
	}

	/* this version of pop_back returns an empty element of T on timeout.
	* Make sure default constructor of T creates a recognizable "empty" element
	*/
	T pop_backNoEx(u32 wait_time_max_ms)
	{
		if (m_signal.wait(wait_time_max_ms)) {
			MutexAutoLock lock(m_mutex);

			T t = std::move(m_queue.back());
			m_queue.pop_back();
			return t;
		}

		return T();
	}

	T pop_backNoEx()
	{
		m_signal.wait();

		MutexAutoLock lock(m_mutex);

		T t = std::move(m_queue.back());
		m_queue.pop_back();
		return t;
	}

protected:
	std::mutex &getMutex() { return m_mutex; }

	std::deque<T> &getQueue() { return m_queue; }

	std::deque<T> m_queue;
	mutable std::mutex m_mutex;
	Semaphore m_signal;
};

/*
	LRU cache
*/

template<typename K, typename V>
class LRUCache
{
public:
	LRUCache(size_t limit, void (*cache_miss)(void *data, const K &key, V *dest),
			void *data)
	{
		m_limit = limit;
		m_cache_miss = cache_miss;
		m_cache_miss_data = data;
	}

	void setLimit(size_t limit)
	{
		m_limit = limit;
		invalidate();
	}

	void invalidate()
	{
		m_map.clear();
		m_queue.clear();
	}

	const V *lookupCache(K key)
	{
		typename cache_type::iterator it = m_map.find(key);
		V *ret;
		if (it != m_map.end()) {
			// found!

			cache_entry_t &entry = it->second;

			ret = &entry.second;

			// update the usage information
			m_queue.erase(entry.first);
			m_queue.push_front(key);
			entry.first = m_queue.begin();
		} else {
			// cache miss -- enter into cache
			cache_entry_t &entry =
				m_map[key];
			ret = &entry.second;
			m_cache_miss(m_cache_miss_data, key, &entry.second);

			// delete old entries
			if (m_queue.size() == m_limit) {
				const K &id = m_queue.back();
				m_map.erase(id);
				m_queue.pop_back();
			}

			m_queue.push_front(key);
			entry.first = m_queue.begin();
		}
		return ret;
	}
private:
	void (*m_cache_miss)(void *data, const K &key, V *dest);
	void *m_cache_miss_data;
	size_t m_limit;
	typedef typename std::template pair<typename std::template list<K>::iterator, V> cache_entry_t;
	typedef std::template map<K, cache_entry_t> cache_type;
	cache_type m_map;
	// we can't use std::deque here, because its iterators get invalidated
	std::list<K> m_queue;
};

/*
	Map that can be safely modified (insertion, deletion) during iteration
	Caveats:
	- you cannot insert null elements
	- you have to check for null elements during iteration, those are ones already deleted
	- size() and empty() don't work during iteration
	- not thread-safe in any way

	How this is implemented:
	- there are two maps: new and real
	- if inserting duration iteration, the value is inserted into the "new" map
	- if deleting during iteration, the value is set to null (to be GC'd later)
	- when iteration finishes the "new" map is merged into the "real" map
*/

template<typename K, typename V>
class ModifySafeMap : public locker<>
{
public:
	// this allows bare pointers but also e.g. std::unique_ptr
	static_assert(std::is_default_constructible<V>::value,
		"Value type must be default constructible");
	static_assert(std::is_constructible<bool, V>::value,
		"Value type must be convertible to bool");
	static_assert(std::is_move_assignable<V>::value,
		"Value type must be move-assignable");

	typedef K key_type;
	typedef V mapped_type;

	ModifySafeMap() {
		// the null value must convert to false and all others to true, but
		// we can't statically check the latter.
		sanity_check(!null_value);
	}
	~ModifySafeMap() {
		assert(!m_iterating);
	}

	// possible to implement but we don't need it
	DISABLE_CLASS_COPY(ModifySafeMap)
	ALLOW_CLASS_MOVE(ModifySafeMap)

	const V &get(const K &key) const {
		const auto lock = lock_shared_rec();
		if (m_iterating) {
			auto it = m_new.find(key);
			if (it != m_new.end())
				return it->second;
		}
		auto it = m_values.find(key);
		// This conditional block was converted from a ternary to ensure no
		// temporary values are created in evaluating the return expression,
		// which could cause a dangling reference.
		if (it != m_values.end()) {
			return it->second;
		} else {
			return null_value;
		}
	}

	void put(const K &key, const V &value) {
		if (!value) {
			assert(false);
			return;
		}
		const auto lock = lock_unique_rec();
		if (m_iterating) {
			auto it = m_values.find(key);
			if (it != m_values.end()) {
				it->second = V();
				m_garbage++;
			}
			m_new[key] = value;
		} else {
			m_values[key] = value;
		}
	}

	void put(const K &key, V &&value) {
		if (!value) {
			assert(false);
			return;
		}
		const auto lock = lock_unique_rec();
		if (m_iterating) {
			auto it = m_values.find(key);
			if (it != m_values.end()) {
				it->second = V();
				m_garbage++;
			}
			m_new[key] = std::move(value);
		} else {
			m_values[key] = std::move(value);
		}
	}

	V take(const K &key) {
		V ret = V();
		const auto lock = lock_unique_rec();
		if (m_iterating) {
			auto it = m_new.find(key);
			if (it != m_new.end()) {
				ret = std::move(it->second);
				m_new.erase(it);
			}
		}
		auto it = m_values.find(key);
		if (it == m_values.end())
			return ret;
		if (!ret)
			ret = std::move(it->second);
		if (m_iterating) {
			it->second = V();
			m_garbage++;
		} else {
			m_values.erase(it);
		}
		return ret;
	}

	bool remove(const K &key) {
		return !!take(key);
	}

	// Warning: not constant-time!
	size_t size() const {
		if (m_iterating) {
			// This is by no means impossible to determine, it's just annoying
			// to code and we happen to not need this.
			return unknown;
		}
		const auto lock = lock_shared_rec();
		assert(m_new.empty());
		if (m_garbage == 0)
			return m_values.size();
		size_t n = 0;
		for (auto &it : m_values)
			n += !it.second ? 0 : 1;
		return n;
	}

	// Warning: not constant-time!
	bool empty() const {
		const auto lock = lock_shared_rec();
		if (m_iterating)
			return false; // maybe
		if (m_garbage == 0)
			return m_values.empty();
		for (auto &it : m_values) {
			if (!!it.second)
				return false;
		}
		return true;
	}

	auto iter() { return IterationHelper(this); }

	void clear() {
		const auto lock = lock_unique_rec();
		if (m_iterating) {
			for (auto &it : m_values)
				it.second = V();
			m_garbage = m_values.size();
		} else {
			m_values.clear();
			m_garbage = 0;
		}
	}

	static inline const V null_value = V();

	// returned by size() if called during iteration
	static constexpr size_t unknown = static_cast<size_t>(-1);

protected:
	void merge_new() {
		assert(!m_iterating);
		if (!m_new.empty()) {
			m_new.merge(m_values); // entries in m_new take precedence
			m_values.clear();
			std::swap(m_values, m_new);
		}
	}

	void collect_garbage() {
		assert(!m_iterating);
		if (m_values.size() < GC_MIN_SIZE || m_garbage < m_values.size() / 2)
			return;
		for (auto it = m_values.begin(); it != m_values.end(); ) {
			if (!it->second)
				it = m_values.erase(it);
			else
				++it;
		}
		m_garbage = 0;
	}

	struct IterationHelper {
		friend class ModifySafeMap<K, V>;
		~IterationHelper() {
			assert(m->m_iterating);
			m->m_iterating--;
			if (!m->m_iterating) {
				m->merge_new();
				m->collect_garbage();
			}
		}

		auto begin() { return m->m_values.cbegin(); }
		auto end() { return m->m_values.cend(); }

	private:
		IterationHelper(ModifySafeMap<K, V> *parent) : m(parent),
			lock{parent->lock_shared_rec()}
		{
			//assert(m->m_iterating < std::numeric_limits<decltype(m_iterating)>::max());
			m->m_iterating++;
		}

		ModifySafeMap<K, V> *m;
		const std::unique_ptr<lock_rec_shared> lock;
	};

private:
	std::map<K, V> m_values;
	std::map<K, V> m_new;
	std::atomic_uint m_iterating = 0;
	// approximate amount of null-placeholders in m_values, reliable for != 0 tests
	size_t m_garbage = 0;

	static constexpr size_t GC_MIN_SIZE = 30;
};
