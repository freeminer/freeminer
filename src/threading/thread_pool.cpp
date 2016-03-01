#include "thread_pool.h"
#include "log.h"
#include "porting.h"

thread_pool::thread_pool(const std::string &name, int priority) :
	m_name(name),
	m_priority(priority) {
	requeststop = false;
};

thread_pool::~thread_pool() {
	join();
};

void thread_pool::func() {
	reg();
	run();
};

void thread_pool::reg(const std::string &name, int priority) {
	if (!name.empty())
		m_name = name;

	porting::setThreadName(m_name.c_str());
	g_logger.registerThread(m_name);

	if (priority)
		m_priority = priority;
	if (m_priority)
		porting::setThreadPriority(m_priority);
};

void thread_pool::start (int n) {
	requeststop = false;
	for(int i = 0; i < n; ++i)
		workers.emplace_back(std::thread(&thread_pool::func, this));
}

void thread_pool::stop () {
	requeststop = true;
}

void thread_pool::join () {
	stop();
	for (auto & worker : workers)
		worker.join();
	workers.clear();
}

void thread_pool::restart (int n) {
	join();
	start(n);
}
void thread_pool::reanimate(int n) {
	if (workers.empty())
		start(n);
}


// JThread compat:
bool thread_pool::stopRequested() {
	return requeststop;
}
bool thread_pool::isRunning() {
	if (requeststop)
		join();
	return !workers.empty();
}
void thread_pool::wait() {
	join();
};
void thread_pool::kill() {
	join();
};
void * thread_pool::run() {
	return nullptr;
};

bool thread_pool::isCurrentThread() {
	auto thread_me = std::hash<std::thread::id>()(std::this_thread::get_id());
	for (auto & worker : workers)
		if (thread_me == std::hash<std::thread::id>()(worker.get_id()))
			return true;
	return false;
}
