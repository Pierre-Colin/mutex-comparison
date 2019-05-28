#include <array>
#include <atomic>
#include <chrono>
#include <exception>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#define TEST_NUM 1000000

class assert_fail : private std::exception {
	int c;

	public:
	assert_fail(const int c) noexcept : c(c) {}
	~assert_fail(void) {}
	const char *what(void) const noexcept {
		std::stringstream s;
		s << "count = " << c;
		return s.str().c_str();
	}
};

class mutex {
	public:
	virtual ~mutex(void) noexcept {};
	virtual void lock(const size_t i) noexcept = 0;
	virtual void unlock(const size_t i) noexcept = 0;
};

class dekker_mutex : public mutex {
	std::array<std::atomic<bool>, 2> want;
	volatile size_t turn;

	public:
	dekker_mutex(void) : want(), turn(0) {
		for (auto &e : want)
			e.store(false, std::memory_order_relaxed);
	}

	virtual ~dekker_mutex(void) noexcept {}

	void lock(const size_t i) noexcept {
		want.at(i).store(true, std::memory_order_seq_cst);
		while (want[1 - i].load(std::memory_order_seq_cst)) {
			if (turn != i) {
				want[i].store(false, std::memory_order_seq_cst);
				while (turn != i);
				want[i].store(true, std::memory_order_seq_cst);
			}
		}
	}

	void unlock(const size_t i) noexcept {
		turn = 1 - i;
		want.at(i).store(false, std::memory_order_seq_cst);
	}
};

class peterson_mutex : public mutex {
	std::array<std::atomic<bool>, 2> flag;
	size_t turn;

	public:
	peterson_mutex(void) noexcept : flag(), turn(0) {}
	virtual ~peterson_mutex(void) noexcept {}

	void lock(const size_t i) noexcept {
		flag.at(i).store(true);
		turn = 1 - i;
		while (flag[1 - i].load(std::memory_order_relaxed) && turn != i);
	}

	void unlock(const size_t i) noexcept {
		flag[i].store(false, std::memory_order_relaxed);
	}
};

class standard_mutex : public mutex {
	std::mutex m;

	public:
	standard_mutex(void) : m() {}
	virtual ~standard_mutex(void) noexcept {}

	void lock(const size_t i) noexcept {
		(void) i;
		m.lock();
	}

	void unlock(const size_t i) noexcept {
		(void) i;
		m.unlock();
	}
};

class feather_mutex : public mutex {
	std::atomic_flag m;

	public:
	feather_mutex(void) : m() {}
	virtual ~feather_mutex(void) noexcept {}
	
	void lock(const size_t i) noexcept {
		(void) i;
		while(m.test_and_set(std::memory_order_relaxed))
			std::this_thread::yield();
	}

	void unlock(const size_t i) noexcept {
		(void) i;
		m.clear(std::memory_order_relaxed);
	}
};

static volatile int count = 0;

static void
assert_count(const int c)
{
	if (count != c)
		throw assert_fail(c);
}

static void
loop(const size_t t, mutex *m)
{
	for (unsigned int i = 0; i < TEST_NUM; i++) {
		m->lock(t);
		assert_count(0);
		count++;
		assert_count(1);
		count--;
		assert_count(0);
		m->unlock(t);
	}
}

static double
test(const size_t n, mutex &m)
{
	std::vector<std::thread> t;
	const clock_t start = std::clock();
	for (size_t i = 0; i < n; ++i)
		t.push_back(std::thread(loop, i, &m));
	for (auto &x : t)
		x.join();
	return ((double) (clock() - start)) / CLOCKS_PER_SEC;
}

int
main(void)
{
	try {
		standard_mutex s;
		std::cout << "Standard: " << test(2, s) << std::endl;
		feather_mutex f;
		std::cout << "Feather: " << test(2, f) << std::endl;
		peterson_mutex p;
		std::cout << "Peterson: " << test(2, p) << std::endl;
		dekker_mutex d;
		std::cout << "Dekker: " << test(2, d) << std::endl;
		return EXIT_SUCCESS;
	} catch (assert_fail &e) {
		std::cerr << "Data race: " << e.what() << std::endl;
		return EXIT_FAILURE;
	} catch (std::exception &e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}
}
