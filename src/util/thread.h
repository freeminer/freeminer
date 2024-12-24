// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "irrlichttypes.h"
#include "threading/thread.h"
#include "threading/mutex_auto_lock.h"
#include "porting.h"
#include "log.h"
#include "container.h"

template<typename T>
class MutexedVariable
{
public:
	MutexedVariable(const T &value):
		m_value(value)
	{}

	T get()
	{
		MutexAutoLock lock(m_mutex);
		return m_value;
	}

	void set(const T &value)
	{
		MutexAutoLock lock(m_mutex);
		m_value = value;
	}

	// You pretty surely want to grab the lock when accessing this
	T m_value;
private:
	std::mutex m_mutex;
};

/*
	A single worker thread - multiple client threads queue framework.
*/
template<typename Key, typename T, typename Caller, typename CallerData>
class GetResult {
public:
	Key key;
	T item;
	std::pair<Caller, CallerData> caller;
};

template<typename Key, typename T, typename Caller, typename CallerData>
class ResultQueue : public MutexedQueue<GetResult<Key, T, Caller, CallerData> > {
};

template<typename Caller, typename Data, typename Key, typename T>
class CallerInfo {
public:
	Caller caller;
	Data data;
	ResultQueue<Key, T, Caller, Data> *dest;
};

template<typename Key, typename T, typename Caller, typename CallerData>
class GetRequest {
public:
	GetRequest() = default;
	~GetRequest() = default;

	GetRequest(const Key &a_key): key(a_key)
	{
	}

	Key key;
	std::list<CallerInfo<Caller, CallerData, Key, T> > callers;
};

/**
 * Notes for RequestQueue usage
 * @param Key unique key to identify a request for a specific resource
 * @param T ?
 * @param Caller unique id of calling thread
 * @param CallerData data passed back to caller
 */
template<typename Key, typename T, typename Caller, typename CallerData>
class RequestQueue {
public:
	bool empty()
	{
		return m_queue.empty();
	}

	void add(const Key &key, Caller caller, CallerData callerdata,
		ResultQueue<Key, T, Caller, CallerData> *dest)
	{
		typename std::deque<GetRequest<Key, T, Caller, CallerData> >::iterator i;
		typename std::list<CallerInfo<Caller, CallerData, Key, T> >::iterator j;

		{
			MutexAutoLock lock(m_queue.getMutex());

			/*
				If the caller is already on the list, only update CallerData
			*/
			for (i = m_queue.getQueue().begin(); i != m_queue.getQueue().end(); ++i) {
				GetRequest<Key, T, Caller, CallerData> &request = *i;
				if (request.key != key)
					continue;

				for (j = request.callers.begin(); j != request.callers.end(); ++j) {
					CallerInfo<Caller, CallerData, Key, T> &ca = *j;
					if (ca.caller == caller) {
						ca.data = callerdata;
						return;
					}
				}

				CallerInfo<Caller, CallerData, Key, T> ca;
				ca.caller = caller;
				ca.data = callerdata;
				ca.dest = dest;
				request.callers.push_back(ca);
				return;
			}
		}

		/*
			Else add a new request to the queue
		*/

		GetRequest<Key, T, Caller, CallerData> request;
		request.key = key;
		CallerInfo<Caller, CallerData, Key, T> ca;
		ca.caller = caller;
		ca.data = callerdata;
		ca.dest = dest;
		request.callers.push_back(ca);

		m_queue.push_back(request);
	}

	GetRequest<Key, T, Caller, CallerData> pop(unsigned int timeout_ms)
	{
		return m_queue.pop_front(timeout_ms);
	}

	GetRequest<Key, T, Caller, CallerData> pop()
	{
		return m_queue.pop_frontNoEx();
	}

	void pushResult(GetRequest<Key, T, Caller, CallerData> req, T res)
	{
		for (typename std::list<CallerInfo<Caller, CallerData, Key, T> >::iterator
				i = req.callers.begin();
				i != req.callers.end(); ++i) {
			CallerInfo<Caller, CallerData, Key, T> &ca = *i;

			GetResult<Key,T,Caller,CallerData> result;

			result.key = req.key;
			result.item = res;
			result.caller.first = ca.caller;
			result.caller.second = ca.data;

			ca.dest->push_back(result);
		}
	}

private:
	MutexedQueue<GetRequest<Key, T, Caller, CallerData> > m_queue;
};

class UpdateThread : public Thread
{
public:
	UpdateThread(const std::string &name) : Thread(name + "Update") {}
	~UpdateThread() = default;

	void deferUpdate() { m_update_sem.post(); }

	void stop()
	{
		Thread::stop();

		// give us a nudge
		m_update_sem.post();
	}

	void *run()
	{
		BEGIN_DEBUG_EXCEPTION_HANDLER

		while (!stopRequested()) {
			m_update_sem.wait();
			// Set semaphore to 0
			while (m_update_sem.wait(0));

			if (stopRequested()) break;

			doUpdate();
		}

		END_DEBUG_EXCEPTION_HANDLER

		return NULL;
	}

protected:
	virtual void doUpdate() = 0;

private:
	Semaphore m_update_sem;
};
