#ifndef UTIL_THREAD_POOL_HEADER
#define UTIL_THREAD_POOL_HEADER

#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>
#include <vector>
#include <condition_variable>
#include <string>

class thread_pool {
public:

	std::mutex queue_mutex;
	std::condition_variable condition;
	std::vector<std::thread> workers;
	std::atomic_bool requeststop;

	thread_pool(const std::string &name="Unnamed");
	virtual ~thread_pool();

	virtual void func();

	void reg (const std::string &name, int priority = 0);
	void start (int n = 1);
	void restart (int n = 1);
	void stop ();
	void join ();

// Thread compat:

	bool stopRequested();
	bool isRunning();
	void wait();
	void kill();
	virtual void * run() = 0;
	bool isSameThread();
protected:
	std::string name;
};


#endif
