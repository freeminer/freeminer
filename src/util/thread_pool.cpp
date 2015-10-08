#include <util/thread_pool.h>
#include <log.h>
#include <porting.h>

thread_pool::thread_pool(const std::string &name) :
	name(name) {
	requeststop = false;
};

thread_pool::~thread_pool() {
	join();
};

void thread_pool::func() {
	reg(name);
	run();
};

void thread_pool::reg(const std::string &name, int priority) {
	if (!name.empty()) {
		porting::setThreadName(name.c_str());
		log_register_thread(name);
	}
	if (priority)
		porting::setThreadPriority(priority);
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

bool thread_pool::isSameThread() {
	auto thread_me = std::hash<std::thread::id>()(std::this_thread::get_id());
	for (auto & worker : workers)
		if (thread_me == std::hash<std::thread::id>()(worker.get_id()))
			return true;
	return false;
}
