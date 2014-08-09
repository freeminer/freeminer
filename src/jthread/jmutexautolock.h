#ifndef JMUTEXAUTOLOCK_H

#define JMUTEXAUTOLOCK_H

#include <mutex>

typedef std::unique_lock<std::mutex> JMutexAutoLock;

#endif // JMUTEXAUTOLOCK_H
