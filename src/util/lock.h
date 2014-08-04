#ifndef UTIL_LOCK_HEADER
#define UTIL_LOCK_HEADER

#include <mutex>
#include <atomic>
#include <thread>
#include <memory>
#include <chrono>

#if USE_BOOST // not finished

//#include <ctime>
#include <boost/thread.hpp>
//#include <boost/thread/locks.hpp>
typedef boost::shared_mutex try_shared_mutex;
typedef boost::shared_lock<try_shared_mutex> try_shared_lock;
typedef boost::unique_lock<try_shared_mutex> unique_lock;
#define DEFER_LOCK boost::defer_lock
#define LOCK_TWO 1

#elif CMAKE_HAVE_SHARED_MUTEX
//#elif __cplusplus >= 201305L

#include <shared_mutex>
typedef std::shared_timed_mutex try_shared_mutex;
typedef std::shared_lock<try_shared_mutex> try_shared_lock;
typedef std::unique_lock<try_shared_mutex> unique_lock;
#define DEFER_LOCK std::defer_lock
#define LOCK_TWO 1

#else

typedef std::timed_mutex try_shared_mutex;
//typedef std::mutex try_shared_mutex;
typedef std::unique_lock<try_shared_mutex> try_shared_lock;
typedef std::unique_lock<try_shared_mutex> unique_lock;
#define DEFER_LOCK std::defer_lock
#endif


// http://stackoverflow.com/questions/4792449/c0x-has-no-semaphores-how-to-synchronize-threads
/* uncomment when need
#include <condition_variable>
class semaphore {
private:
	std::mutex mtx;
	std::condition_variable cv;
	int count;

public:
	semaphore(int count_ = 0):count(count_){;}
	void notify() {
	std::unique_lock<std::mutex> lck(mtx);
		++count;
		cv.notify_one();
	}
	void wait() {
		std::unique_lock<std::mutex> lck(mtx);
		while(count == 0){
			cv.wait(lck);
		}
		count--;
	}
};
*/

template<class T>
class lock_rec {
public:
	T * lock;
	std::atomic<std::size_t> & thread_id;
	lock_rec(T * lock_, std::atomic<std::size_t> & thread_id_);
	lock_rec(T * lock_, std::atomic<std::size_t> & thread_id_, std::chrono::milliseconds ms);
	~lock_rec();
	bool owns_lock();
};

class locker {
public:
	try_shared_mutex mtx;
	//semaphore sem;
	std::atomic<std::size_t> thread_id;

	locker();
	std::unique_ptr<unique_lock> lock_unique();
	std::unique_ptr<unique_lock> lock_unique(std::chrono::milliseconds ms);
	std::unique_ptr<try_shared_lock> lock_shared();
	std::unique_ptr<try_shared_lock> lock_shared(std::chrono::milliseconds ms);
	std::unique_ptr<lock_rec<unique_lock>> lock_unique_rec();
	std::unique_ptr<lock_rec<unique_lock>> lock_unique_rec(std::chrono::milliseconds ms);
	std::unique_ptr<lock_rec<try_shared_lock>> lock_shared_rec();
	std::unique_ptr<lock_rec<try_shared_lock>> lock_shared_rec(std::chrono::milliseconds ms);
};


#include <map>
template < class Key, class T, class Compare = std::less<Key>,
         class Allocator = std::allocator<std::pair<const Key, T> >>
class shared_map: public std::map<Key, T, Compare, Allocator>,
	public locker {
public:

	typedef Key                                      key_type;
	typedef T                                        mapped_type;
	typedef Allocator                                allocator_type;
	typedef typename allocator_type::size_type       size_type;
	typedef typename std::map<Key, T, Compare, Allocator> full_type;
	typedef typename full_type::const_iterator const_iterator;
	typedef typename full_type::iterator iterator;

	mapped_type& get(const key_type& k) {
		auto lock = lock_shared_rec();
		return full_type::operator[](k);
	}

	void set(const key_type& k, const mapped_type& v) {
		auto lock = lock_unique_rec();
		full_type::operator[](k) = v;
	}

	bool      empty() {
		auto lock = lock_shared_rec();
		return full_type::empty();
	}

	size_type size() {
		auto lock = lock_shared_rec();
		return full_type::size();
	}

	size_type count(const key_type& k) {
		auto lock = lock_shared_rec();
		return full_type::count(k);
	}

	mapped_type& operator[](const key_type& k) = delete;

	mapped_type& operator[](key_type&& k) = delete;

	typename full_type::iterator  erase(const_iterator position) {
		auto lock = lock_unique_rec();
		return full_type::erase(position);
	}

	typename full_type::iterator  erase(iterator position) {
		auto lock = lock_unique_rec();
		return full_type::erase(position);
	}

	size_type erase(const key_type& k) {
		auto lock = lock_unique_rec();
		return full_type::erase(k);
	}

	typename full_type::iterator  erase(const_iterator first, const_iterator last) {
		auto lock = lock_unique_rec();
		return full_type::erase(first, last);
	}

	void clear() {
		auto lock = lock_unique_rec();
		full_type::clear();
	}
};

#endif
