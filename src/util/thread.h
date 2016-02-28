/*
util/thread.h
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
*/

/*
This file is part of Freeminer.

Freeminer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Freeminer  is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Freeminer.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef UTIL_THREAD_HEADER
#define UTIL_THREAD_HEADER

#include "../irrlichttypes.h"
#include "../threading/thread.h"
#include "../threading/mutex.h"
#include "../threading/mutex_auto_lock.h"
#include "porting.h"
#include "log.h"
#include "../threading/thread_pool.h"

template<typename T>
class MutexedVariable {
public:
	MutexedVariable(T value):
		m_value(value)
	{}

	T get()
	{
		MutexAutoLock lock(m_mutex);
		return m_value;
	}

	void set(T value)
	{
		MutexAutoLock lock(m_mutex);
		m_value = value;
	}

	// You'll want to grab this in a SharedPtr
	MutexAutoLock *getLock()
	{
		return new MutexAutoLock(m_mutex);
	}

	// You pretty surely want to grab the lock when accessing this
	T m_value;

private:
	Mutex m_mutex;
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
	GetRequest() {}
	~GetRequest() {}

	GetRequest(Key a_key) {
		key = a_key;
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

	void add(Key key, Caller caller, CallerData callerdata,
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

class UpdateThread : public thread_pool
{
public:
	UpdateThread(const std::string &name) : thread_pool(name + "Update") {}
	~UpdateThread() {}

	void deferUpdate() { m_update_sem.post(); }

	void stop()
	{
		thread_pool::stop();

		// give us a nudge
		m_update_sem.post();
	}

	void *run()
	{
		porting::setThreadPriority(30);

		DSTACK(FUNCTION_NAME);
		BEGIN_DEBUG_EXCEPTION_HANDLER

		while (!stopRequested()) {
			EXCEPTION_HANDLER_BEGIN;
			m_update_sem.wait(1000);
			// Set semaphore to 0
			while (m_update_sem.wait(0));

			if (stopRequested()) break;

			doUpdate();
			EXCEPTION_HANDLER_END;
		}

		END_DEBUG_EXCEPTION_HANDLER

		return NULL;
	}

protected:
	virtual void doUpdate() = 0;

private:
	Semaphore m_update_sem;
};

#endif

