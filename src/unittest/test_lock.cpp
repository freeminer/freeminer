#include "test.h"

#include "threading/lock.h"

class TestLock : public TestBase
{
public:
	TestLock() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestLock"; }
	void runTests(IGameDef *gamedef);

	void testlock1();
};

static TestLock g_test_instance;

void TestLock::runTests(IGameDef *gamedef)
{
	TEST(testlock1);
}

/*
class SimpleTestThread : public Thread {
public:
	SimpleTestThread(unsigned int interval) :
		Thread("SimpleTest"),
		m_interval(interval)
	{
	}

private:
	void *run()
	{
		void *retval = this;

		if (isCurrentThread() == false)
			retval = (void *)0xBAD;

		while (!stopRequested())
			sleep_ms(m_interval);

		return retval;
	}

	unsigned int m_interval;
};
*/

void TestLock::testlock1()
{
	// void *thread_retval;
	//SimpleTestThread *thread = new SimpleTestThread(25);

	class lockable : public locker<>
	{
	};

	{
		lockable l1;
		{
			const auto lock1 = l1.try_lock_shared_rec();
			UASSERT(lock1->owns_lock());
			{
				const auto lock2 = l1.try_lock_shared_rec();
				UASSERT(lock2->owns_lock());
			}
		}

		{
			const auto lock1 = l1.try_lock_shared();
			UASSERT(lock1->owns_lock());
			{
				const auto lock2 = l1.try_lock_shared();
				UASSERT(!lock2->owns_lock());
			}
		}

		{
			const auto lock1 = l1.try_lock_unique_rec();
			UASSERT(lock1->owns_lock());
			{
				const auto lock2 = l1.try_lock_unique_rec();
				UASSERT(lock2->owns_lock());
			}
		}

		{
			const auto lock1 = l1.try_lock_unique();
			UASSERT(lock1->owns_lock());
			{
				const auto lock2 = l1.try_lock_unique();
				UASSERT(!lock2->owns_lock());
			}
		}
	}

	class lockable_shared : public shared_locker
	{
	};
	{
		lockable_shared l1;
		{
			const auto lock1 = l1.try_lock_shared_rec();
			UASSERT(lock1->owns_lock());
			{
				const auto lock2 = l1.try_lock_shared_rec();
				UASSERT(lock2->owns_lock());
			}
		}

		{
			const auto lock1 = l1.try_lock_shared();
			UASSERT(lock1->owns_lock());
			{
				const auto lock2 = l1.try_lock_shared();
				UASSERT(lock2->owns_lock());
			}
		}

		{
			const auto lock1 = l1.try_lock_unique_rec();
			UASSERT(lock1->owns_lock());
			{
				const auto lock2 = l1.try_lock_unique_rec();
				UASSERT(lock2->owns_lock());
			}
		}

		{
			const auto lock1 = l1.try_lock_unique();
			UASSERT(lock1->owns_lock());
			{
				const auto lock2 = l1.try_lock_unique();
				UASSERT(!lock2->owns_lock());
			}
		}
	}

	//delete thread;
}
