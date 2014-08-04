#ifndef UTIL_THREAD_POOL_HEADER
#define UTIL_THREAD_POOL_HEADER

#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>
#include <vector>
#include <condition_variable>

class thread_pool {
public:

	std::mutex queue_mutex;
	std::condition_variable condition;
	std::vector<std::thread> workers;
	std::atomic_bool requeststop;

	thread_pool();
	virtual ~thread_pool();

	virtual void func();

	void start (int n = 1);
	void stop ();
	void join ();

// JThread compat:
	void ThreadStarted();
	bool StopRequested();
	bool IsRunning();
	int Start(int n = 1);
	void Stop();
	void Wait();
	void Kill();
	virtual void * Thread();
};


#endif
