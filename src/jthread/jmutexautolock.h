#ifndef JMUTEXAUTOLOCK_H

#define JMUTEXAUTOLOCK_H

#include <mutex>

typedef std::lock_guard<std::mutex> JMutexAutoLock;

#endif // JMUTEXAUTOLOCK_H
