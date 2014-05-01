/*
httpfetch.cpp
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
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

#include "socket.h" // for select()
#include "porting.h" // for sleep_ms(), get_sysinfo()
#include "httpfetch.h"
#include <iostream>
#include <sstream>
#include <list>
#include <map>
#include <errno.h>
#include "jthread/jevent.h"
#include "config.h"
#include "exceptions.h"
#include "debug.h"
#include "log.h"
#include "util/container.h"
#include "util/thread.h"
#include "version.h"
#include "main.h"
#include "settings.h"

JMutex g_httpfetch_mutex;
std::map<unsigned long, std::list<HTTPFetchResult> > g_httpfetch_results;

HTTPFetchRequest::HTTPFetchRequest()
{
	url = "";
	caller = HTTPFETCH_DISCARD;
	request_id = 0;
	timeout = g_settings->getS32("curl_timeout");
	connect_timeout = timeout;
	
	useragent = std::string("Minetest/") + minetest_version_hash + " (" + porting::get_sysinfo() + ")";
}


static void httpfetch_deliver_result(const HTTPFetchResult &fetchresult)
{
	unsigned long caller = fetchresult.caller;
	if (caller != HTTPFETCH_DISCARD) {
		JMutexAutoLock lock(g_httpfetch_mutex);
		g_httpfetch_results[caller].push_back(fetchresult);
	}
}

static void httpfetch_request_clear(unsigned long caller);

unsigned long httpfetch_caller_alloc()
{
	JMutexAutoLock lock(g_httpfetch_mutex);

	// Check each caller ID except HTTPFETCH_DISCARD
	const unsigned long discard = HTTPFETCH_DISCARD;
	for (unsigned long caller = discard + 1; caller != discard; ++caller) {
		std::map<unsigned long, std::list<HTTPFetchResult> >::iterator
			it = g_httpfetch_results.find(caller);
		if (it == g_httpfetch_results.end()) {
			verbosestream<<"httpfetch_caller_alloc: allocating "
					<<caller<<std::endl;
			// Access element to create it
			g_httpfetch_results[caller];
			return caller;
		}
	}

	assert("httpfetch_caller_alloc: ran out of caller IDs" == 0);
	return discard;
}

void httpfetch_caller_free(unsigned long caller)
{
	verbosestream<<"httpfetch_caller_free: freeing "
			<<caller<<std::endl;

	httpfetch_request_clear(caller);
	if (caller != HTTPFETCH_DISCARD) {
		JMutexAutoLock lock(g_httpfetch_mutex);
		g_httpfetch_results.erase(caller);
	}
}

bool httpfetch_async_get(unsigned long caller, HTTPFetchResult &fetchresult)
{
	JMutexAutoLock lock(g_httpfetch_mutex);

	// Check that caller exists
	std::map<unsigned long, std::list<HTTPFetchResult> >::iterator
		it = g_httpfetch_results.find(caller);
	if (it == g_httpfetch_results.end())
		return false;

	// Check that result queue is nonempty
	std::list<HTTPFetchResult> &callerresults = it->second;
	if (callerresults.empty())
		return false;

	// Pop first result
	fetchresult = callerresults.front();
	callerresults.pop_front();
	return true;
}

#if USE_CURL
#include <curl/curl.h>
#ifndef _MSC_VER
#include <sys/utsname.h>
#endif

/*
	USE_CURL is on: use cURL based httpfetch implementation
*/

static size_t httpfetch_writefunction(
		char *ptr, size_t size, size_t nmemb, void *userdata)
{
	std::ostringstream *stream = (std::ostringstream*)userdata;
	size_t count = size * nmemb;
	stream->write(ptr, count);
	return count;
}

static size_t httpfetch_discardfunction(
		char *ptr, size_t size, size_t nmemb, void *userdata)
{
	return size * nmemb;
}

class CurlHandlePool
{
	std::list<CURL*> handles;

public:
	CurlHandlePool() {}
	~CurlHandlePool()
	{
		for (std::list<CURL*>::iterator it = handles.begin();
				it != handles.end(); ++it) {
			curl_easy_cleanup(*it);
		}
	}
	CURL * alloc()
	{
		CURL *curl;
		if (handles.empty()) {
			curl = curl_easy_init();
			if (curl == NULL) {
				errorstream<<"curl_easy_init returned NULL"<<std::endl;
			}
		}
		else {
			curl = handles.front();
			handles.pop_front();
		}
		return curl;
	}
	void free(CURL *handle)
	{
		if (handle)
			handles.push_back(handle);
	}
};

struct HTTPFetchOngoing
{
	CurlHandlePool *pool;
	CURL *curl;
	CURLM *multi;
	HTTPFetchRequest request;
	HTTPFetchResult result;
	std::ostringstream oss;
	char *post_fields;
	struct curl_slist *httpheader;

	HTTPFetchOngoing(HTTPFetchRequest request_, CurlHandlePool *pool_):
		pool(pool_),
		curl(NULL),
		multi(NULL),
		request(request_),
		result(request_),
		oss(std::ios::binary),
		httpheader(NULL)
	{
		curl = pool->alloc();
		if (curl != NULL) {
			// Set static cURL options
			curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
			curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
			curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 1);

#if LIBCURL_VERSION_NUM >= 0x071304
			// Restrict protocols so that curl vulnerabilities in
			// other protocols don't affect us.
			// These settings were introduced in curl 7.19.4.
			long protocols =
				CURLPROTO_HTTP |
				CURLPROTO_HTTPS |
				CURLPROTO_FTP |
				CURLPROTO_FTPS;
			curl_easy_setopt(curl, CURLOPT_PROTOCOLS, protocols);
			curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, protocols);
#endif

			// Set cURL options based on HTTPFetchRequest
			curl_easy_setopt(curl, CURLOPT_URL,
					request.url.c_str());
			curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS,
					request.timeout);
			curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS,
					request.connect_timeout);

			if (request.useragent != "")
				curl_easy_setopt(curl, CURLOPT_USERAGENT, request.useragent.c_str());
			else {
				std::string useragent = std::string("Minetest ") + minetest_version_hash;
#ifdef _MSC_VER
				useragent += "Windows";
#else
				struct utsname osinfo;
				uname(&osinfo);
				useragent += std::string(" (") + osinfo.sysname + "; " + osinfo.release + "; " + osinfo.machine + ")";
#endif
				curl_easy_setopt(curl, CURLOPT_USERAGENT, useragent.c_str());
			}
			// Set up a write callback that writes to the
			// ostringstream ongoing->oss, unless the data
			// is to be discarded
			if (request.caller == HTTPFETCH_DISCARD) {
				curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
						httpfetch_discardfunction);
				curl_easy_setopt(curl, CURLOPT_WRITEDATA, NULL);
			}
			else {
				curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
						httpfetch_writefunction);
				curl_easy_setopt(curl, CURLOPT_WRITEDATA, &oss);
			}
			// Set POST (or GET) data
			if (request.post_fields.empty()) {
				curl_easy_setopt(curl, CURLOPT_HTTPGET, 1);
			}
			else {
				curl_easy_setopt(curl, CURLOPT_POST, 1);
				curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,
						request.post_fields.size());
				curl_easy_setopt(curl, CURLOPT_POSTFIELDS,
						request.post_fields.c_str());
				// request.post_fields must now *never* be
				// modified until CURLOPT_POSTFIELDS is cleared
			}
			// Set additional HTTP headers
			for (size_t i = 0; i < request.extra_headers.size(); ++i) {
				httpheader = curl_slist_append(
					httpheader,
					request.extra_headers[i].c_str());
			}
			curl_easy_setopt(curl, CURLOPT_HTTPHEADER, httpheader);
		}
	}

	CURLcode start(CURLM *multi_)
	{
		if (curl == NULL)
			return CURLE_FAILED_INIT;

		if (multi_) {
			// Multi interface (async)
			CURLMcode mres = curl_multi_add_handle(multi_, curl);
			if (mres != CURLM_OK) {
				errorstream<<"curl_multi_add_handle"
					<<" returned error code "<<mres
					<<std::endl;
				return CURLE_FAILED_INIT;
			}
			multi = multi_; // store for curl_multi_remove_handle
			return CURLE_OK;
		}
		else {
			// Easy interface (sync)
			return curl_easy_perform(curl);
		}
	}

	void complete(CURLcode res)
	{
		result.succeeded = (res == CURLE_OK);
		result.timeout = (res == CURLE_OPERATION_TIMEDOUT);
		result.data = oss.str();

		// Get HTTP/FTP response code
		result.response_code = 0;
		if (curl != NULL) {
			if (curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE,
					&result.response_code) != CURLE_OK) {
				//we failed to get a return code make sure it is still 0
				result.response_code = 0;
			}
		}

		if (res != CURLE_OK) {
			infostream<<request.url<<" not found ("
				<<curl_easy_strerror(res)<<")"
				<<" (response code "<<result.response_code<<")"
				<<std::endl;
		}
	}

	~HTTPFetchOngoing()
	{
		if (multi != NULL) {
			CURLMcode mres = curl_multi_remove_handle(multi, curl);
			if (mres != CURLM_OK) {
				errorstream<<"curl_multi_remove_handle"
					<<" returned error code "<<mres
					<<std::endl;
			}
		}

		// Set safe options for the reusable cURL handle
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
				httpfetch_discardfunction);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, NULL);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, NULL);
		if (httpheader != NULL) {
			curl_easy_setopt(curl, CURLOPT_HTTPHEADER, NULL);
			curl_slist_free_all(httpheader);
		}

		// Store the cURL handle for reuse
		pool->free(curl);
	}
};

class CurlFetchThread : public JThread
{
protected:
	enum RequestType {
		RT_FETCH,
		RT_CLEAR,
		RT_WAKEUP,
	};

	struct Request {
		RequestType type;
		HTTPFetchRequest fetchrequest;
		Event *event;
	};

	CURLM *m_multi;
	MutexedQueue<Request> m_requests;
	size_t m_parallel_limit;

	// Variables exclusively used within thread
	std::vector<HTTPFetchOngoing*> m_all_ongoing;
	std::list<HTTPFetchRequest> m_queued_fetches;

public:
	CurlFetchThread(int parallel_limit)
	{
		if (parallel_limit >= 1)
			m_parallel_limit = parallel_limit;
		else
			m_parallel_limit = 1;
	}

	void requestFetch(const HTTPFetchRequest &fetchrequest)
	{
		Request req;
		req.type = RT_FETCH;
		req.fetchrequest = fetchrequest;
		req.event = NULL;
		m_requests.push_back(req);
	}

	void requestClear(unsigned long caller, Event *event)
	{
		Request req;
		req.type = RT_CLEAR;
		req.fetchrequest.caller = caller;
		req.event = event;
		m_requests.push_back(req);
	}

	void requestWakeUp()
	{
		Request req;
		req.type = RT_WAKEUP;
		req.event = NULL;
		m_requests.push_back(req);
	}

protected:
	// Handle a request from some other thread
	// E.g. new fetch; clear fetches for one caller; wake up
	void processRequest(const Request &req)
	{
		if (req.type == RT_FETCH) {
			// New fetch, queue until there are less
			// than m_parallel_limit ongoing fetches
			m_queued_fetches.push_back(req.fetchrequest);

			// see processQueued() for what happens next

		}
		else if (req.type == RT_CLEAR) {
			unsigned long caller = req.fetchrequest.caller;

			// Abort all ongoing fetches for the caller
			for (std::vector<HTTPFetchOngoing*>::iterator
					it = m_all_ongoing.begin();
					it != m_all_ongoing.end();) {
				if ((*it)->request.caller == caller) {
					delete (*it);
					it = m_all_ongoing.erase(it);
				}
				else
					++it;
			}

			// Also abort all queued fetches for the caller
			for (std::list<HTTPFetchRequest>::iterator
					it = m_queued_fetches.begin();
					it != m_queued_fetches.end();) {
				if ((*it).caller == caller)
					it = m_queued_fetches.erase(it);
				else
					++it;
			}
		}
		else if (req.type == RT_WAKEUP) {
			// Wakeup: Nothing to do, thread is awake at this point
		}

		if (req.event != NULL)
			req.event->signal();
	}

	// Start new ongoing fetches if m_parallel_limit allows
	void processQueued(CurlHandlePool *pool)
	{
		while (m_all_ongoing.size() < m_parallel_limit &&
				!m_queued_fetches.empty()) {
			HTTPFetchRequest request = m_queued_fetches.front();
			m_queued_fetches.pop_front();

			// Create ongoing fetch data and make a cURL handle
			// Set cURL options based on HTTPFetchRequest
			HTTPFetchOngoing *ongoing =
				new HTTPFetchOngoing(request, pool);

			// Initiate the connection (curl_multi_add_handle)
			CURLcode res = ongoing->start(m_multi);
			if (res == CURLE_OK) {
				m_all_ongoing.push_back(ongoing);
			}
			else {
				ongoing->complete(res);
				httpfetch_deliver_result(ongoing->result);
				delete ongoing;
			}
		}
	}

	// Process CURLMsg (indicates completion of a fetch)
	void processCurlMessage(CURLMsg *msg)
	{
		// Determine which ongoing fetch the message pertains to
		size_t i = 0;
		bool found = false;
		for (i = 0; i < m_all_ongoing.size(); ++i) {
			if (m_all_ongoing[i]->curl == msg->easy_handle) {
				found = true;
				break;
			}
		}
		if (msg->msg == CURLMSG_DONE && found) {
			// m_all_ongoing[i] succeeded or failed.
			HTTPFetchOngoing *ongoing = m_all_ongoing[i];
			ongoing->complete(msg->data.result);
			httpfetch_deliver_result(ongoing->result);
			delete ongoing;
			m_all_ongoing.erase(m_all_ongoing.begin() + i);
		}
	}

	// Wait for a request from another thread, or timeout elapses
	void waitForRequest(long timeout)
	{
		if (m_queued_fetches.empty()) {
			try {
				Request req = m_requests.pop_front(timeout);
				processRequest(req);
			}
			catch (ItemNotFoundException &e) {}
		}
	}

	// Wait until some IO happens, or timeout elapses
	void waitForIO(long timeout)
	{
		fd_set read_fd_set;
		fd_set write_fd_set;
		fd_set exc_fd_set;
		int max_fd;
		long select_timeout = -1;
		struct timeval select_tv;
		CURLMcode mres;

		FD_ZERO(&read_fd_set);
		FD_ZERO(&write_fd_set);
		FD_ZERO(&exc_fd_set);

		mres = curl_multi_fdset(m_multi, &read_fd_set,
				&write_fd_set, &exc_fd_set, &max_fd);
		if (mres != CURLM_OK) {
			errorstream<<"curl_multi_fdset"
				<<" returned error code "<<mres
				<<std::endl;
			select_timeout = 0;
		}

		mres = curl_multi_timeout(m_multi, &select_timeout);
		if (mres != CURLM_OK) {
			errorstream<<"curl_multi_timeout"
				<<" returned error code "<<mres
				<<std::endl;
			select_timeout = 0;
		}

		// Limit timeout so new requests get through
		if (select_timeout < 0 || select_timeout > timeout)
			select_timeout = timeout;

		if (select_timeout > 0) {
			// in Winsock it is forbidden to pass three empty
			// fd_sets to select(), so in that case use sleep_ms
			if (max_fd != -1) {
				select_tv.tv_sec = select_timeout / 1000;
				select_tv.tv_usec = (select_timeout % 1000) * 1000;
				int retval = select(max_fd + 1, &read_fd_set,
						&write_fd_set, &exc_fd_set,
						&select_tv);
				if (retval == -1) {
					#ifdef _WIN32
					errorstream<<"select returned error code "
						<<WSAGetLastError()<<std::endl;
					#else
					errorstream<<"select returned error code "
						<<errno<<std::endl;
					#endif
				}
			}
			else {
				sleep_ms(select_timeout);
			}
		}
	}

	void * Thread()
	{
		ThreadStarted();
		log_register_thread("CurlFetchThread");
		DSTACK(__FUNCTION_NAME);

		porting::setThreadName("CurlFetchThread");

		CurlHandlePool pool;

		m_multi = curl_multi_init();
		if (m_multi == NULL) {
			errorstream<<"curl_multi_init returned NULL\n";
			return NULL;
		}

		assert(m_all_ongoing.empty());

		while (!StopRequested()) {
			BEGIN_DEBUG_EXCEPTION_HANDLER

			/*
				Handle new async requests
			*/

			while (!m_requests.empty()) {
				Request req = m_requests.pop_frontNoEx();
				processRequest(req);
			}
			processQueued(&pool);

			/*
				Handle ongoing async requests
			*/

			int still_ongoing = 0;
			while (curl_multi_perform(m_multi, &still_ongoing) ==
					CURLM_CALL_MULTI_PERFORM)
				/* noop */;

			/*
				Handle completed async requests
			*/
			if (still_ongoing < (int) m_all_ongoing.size()) {
				CURLMsg *msg;
				int msgs_in_queue;
				msg = curl_multi_info_read(m_multi, &msgs_in_queue);
				while (msg != NULL) {
					processCurlMessage(msg);
					msg = curl_multi_info_read(m_multi, &msgs_in_queue);
				}
			}

			/*
				If there are ongoing requests, wait for data
				(with a timeout of 100ms so that new requests
				can be processed).

				If no ongoing requests, wait for a new request.
				(Possibly an empty request that signals
				that the thread should be stopped.)
			*/
			if (m_all_ongoing.empty())
				waitForRequest(100000000);
			else
				waitForIO(100);

			END_DEBUG_EXCEPTION_HANDLER(errorstream)
		}

		// Call curl_multi_remove_handle and cleanup easy handles
		for (size_t i = 0; i < m_all_ongoing.size(); ++i) {
			delete m_all_ongoing[i];
		}
		m_all_ongoing.clear();

		m_queued_fetches.clear();

		CURLMcode mres = curl_multi_cleanup(m_multi);
		if (mres != CURLM_OK) {
			errorstream<<"curl_multi_cleanup"
				<<" returned error code "<<mres
				<<std::endl;
		}

		return NULL;
	}
};

CurlFetchThread *g_httpfetch_thread = NULL;

void httpfetch_init(int parallel_limit)
{
	verbosestream<<"httpfetch_init: parallel_limit="<<parallel_limit
			<<std::endl;

	CURLcode res = curl_global_init(CURL_GLOBAL_DEFAULT);
	assert(res == CURLE_OK);

	g_httpfetch_thread = new CurlFetchThread(parallel_limit);
}

void httpfetch_cleanup()
{
	verbosestream<<"httpfetch_cleanup: cleaning up"<<std::endl;

	g_httpfetch_thread->Stop();
	g_httpfetch_thread->requestWakeUp();
	g_httpfetch_thread->Wait();
	delete g_httpfetch_thread;

	curl_global_cleanup();
}

void httpfetch_async(const HTTPFetchRequest &fetchrequest)
{
	g_httpfetch_thread->requestFetch(fetchrequest);
	if (!g_httpfetch_thread->IsRunning())
		g_httpfetch_thread->Start();
}

static void httpfetch_request_clear(unsigned long caller)
{
	if (g_httpfetch_thread->IsRunning()) {
		Event event;
		g_httpfetch_thread->requestClear(caller, &event);
		event.wait();
	}
	else {
		g_httpfetch_thread->requestClear(caller, NULL);
	}
}

void httpfetch_sync(const HTTPFetchRequest &fetchrequest,
		HTTPFetchResult &fetchresult)
{
	// Create ongoing fetch data and make a cURL handle
	// Set cURL options based on HTTPFetchRequest
	CurlHandlePool pool;
	HTTPFetchOngoing ongoing(fetchrequest, &pool);
	// Do the fetch (curl_easy_perform)
	CURLcode res = ongoing.start(NULL);
	// Update fetchresult
	ongoing.complete(res);
	fetchresult = ongoing.result;
}

#else  // USE_CURL

/*
	USE_CURL is off:

	Dummy httpfetch implementation that always returns an error.
*/

void httpfetch_init(int parallel_limit)
{
}

void httpfetch_cleanup()
{
}

void httpfetch_async(const HTTPFetchRequest &fetchrequest)
{
	errorstream<<"httpfetch_async: unable to fetch "<<fetchrequest.url
			<<" because USE_CURL=0"<<std::endl;

	HTTPFetchResult fetchresult(fetchrequest); // sets succeeded = false etc.
	httpfetch_deliver_result(fetchresult);
}

static void httpfetch_request_clear(unsigned long caller)
{
}

void httpfetch_sync(const HTTPFetchRequest &fetchrequest,
		HTTPFetchResult &fetchresult)
{
	errorstream<<"httpfetch_sync: unable to fetch "<<fetchrequest.url
			<<" because USE_CURL=0"<<std::endl;

	fetchresult = HTTPFetchResult(fetchrequest); // sets succeeded = false etc.
}

#endif  // USE_CURL
