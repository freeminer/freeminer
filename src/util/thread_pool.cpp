#include <util/thread_pool.h>
#include <log.h>

thread_pool::thread_pool() {
	requeststop = false;
};

thread_pool::~thread_pool() {
	join();
};

void thread_pool::func() {
	Thread();
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
void thread_pool::ThreadStarted() {
};
bool thread_pool::StopRequested() {
	return requeststop;
}
bool thread_pool::IsRunning() {
	if (requeststop)
		join();
	return !workers.empty();
}
int thread_pool::Start(int n) {
	start(n);
	return 0;
};
void thread_pool::Stop() {
	stop();
}
void thread_pool::Wait() {
	join();
};
void thread_pool::Kill() {
	join();
};
void * thread_pool::Thread() {
	return nullptr;
};

bool thread_pool::IsSameThread() {
	auto thread_me = std::hash<std::thread::id>()(std::this_thread::get_id());
	for (auto & worker : workers)
		if (thread_me == std::hash<std::thread::id>()(worker.get_id()))
			return true;
	return false;
}
