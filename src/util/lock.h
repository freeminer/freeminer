
#ifndef UTIL_LOCK_HEADER
#define UTIL_LOCK_HEADER

#include <mutex>
#include <atomic>

#if __cplusplus >= 201305L
#include <shared_mutex>
typedef  std::shared_lock try_shared_lock;
typedef  std::shared_timed_mutex<try_shared_mutex> try_shared_mutex;
#else
typedef  std::timed_mutex try_shared_mutex;
typedef  std::unique_lock<try_shared_mutex> try_shared_lock;
//#define std::shared_lock std::unique_lock
//#define std::shared_timed_mutex std::timed_mutex
#endif

typedef  std::unique_lock<try_shared_mutex> unique_lock;






// http://stackoverflow.com/questions/4792449/c0x-has-no-semaphores-how-to-synchronize-threads
#include <condition_variable>
//using namespace std;

class semaphore{
private:
    std::mutex mtx;
    std::condition_variable cv;
    int count;

public:
    semaphore(int count_ = 0):count(count_){;}
    void notify()
    {
        std::unique_lock<std::mutex> lck(mtx);
        ++count;
        cv.notify_one();
    }
    void wait()
    {
        std::unique_lock<std::mutex> lck(mtx);

        while(count == 0){
            cv.wait(lck);
        }
        count--;
    }
};





#endif
