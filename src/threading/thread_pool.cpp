#include "thread_pool.h"
#include "fm_porting.h"
#include "log.h"
#include "porting.h"

thread_pool::thread_pool(const std::string &name, int priority) :
		m_name(name), m_priority(priority)
{
	request_stop = false;
};

thread_pool::~thread_pool()
{
	join();
};

void thread_pool::func()
{
	reg();
	run();
};

void thread_pool::reg(const std::string &name, int priority)
{
	if (!name.empty())
		m_name = name;

	porting::setThreadName(m_name.c_str());
	g_logger.registerThread(m_name);

	if (priority)
		m_priority = priority;
	if (m_priority)
		porting::setThreadPriority(m_priority);
};

void thread_pool::start(const size_t n)
{
#if !NDEBUG
	infostream << "start thread " << m_name << " n=" << n << std::endl;
#endif
	request_stop = false;
	for (size_t i = 0; i < n; ++i) {
		workers.emplace_back(&thread_pool::func, this);
	}
}

void thread_pool::stop()
{
	request_stop = true;
}

void thread_pool::join()
{
	stop();
	for (auto &worker : workers) {
		try {
			if (worker.joinable()) {
				worker.join();
			}
		} catch (...) {
		}
	}
	workers.clear();
}

void thread_pool::restart(size_t n)
{
	join();
	start(n);
}
void thread_pool::reanimate(size_t n)
{
	if (workers.empty()) {
		start(n);
	}
}

void thread_pool::sleep(const int seconds)
{
	for (int i = 0; i <= seconds; ++i) {
		std::this_thread::sleep_for(std::chrono::seconds(1));
		if (request_stop) {
			return;
		}
	}
}

// JThread compat:
bool thread_pool::stopRequested()
{
	return request_stop;
}
bool thread_pool::isRunning()
{
	return !workers.empty();
}
void thread_pool::wait()
{
	join();
};
void thread_pool::kill()
{
	join();
};
void *thread_pool::run()
{
	return nullptr;
};

bool thread_pool::isCurrentThread()
{
	auto thread_me = std::hash<std::thread::id>()(std::this_thread::get_id());
	for (auto &worker : workers)
		if (thread_me == std::hash<std::thread::id>()(worker.get_id()))
			return true;
	return false;
}
