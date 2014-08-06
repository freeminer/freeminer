#ifndef JMUTEX_H

#define JMUTEX_H

#include <mutex>

class JMutex : public std::mutex {
public:

	int Lock() {
		lock();
		return 0;
	};

	int Unlock() {
		unlock();
		return 0;
	};
};

#endif // JMUTEX_H
