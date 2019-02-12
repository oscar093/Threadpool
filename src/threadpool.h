
#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <future>
#include <thread>
#include <vector>
#include <queue>
#include <functional>
#include <utility>

using uint = unsigned int;

class ThreadPool {

	std::vector<std::thread> pool = std::vector<std::thread>();
	std::queue<std::function<void()>> jobs = std::queue<std::function<void()>>();
	std::mutex q_mutex;
	std::condition_variable cv;
	bool is_running;
public:
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool(const ThreadPool&&) = delete;
	ThreadPool& operator = (const ThreadPool&) = delete;
	ThreadPool& operator = (ThreadPool&&) = delete;

	ThreadPool(unsigned int nbrOfThreads);
	template <typename TF, class... Args>
	auto enqueue(TF&& f, Args&&... args) -> std::future<decltype(f(args...))>;
	void start();
	void work();
	void shutdown();
};

ThreadPool::ThreadPool(uint nbrOfThreads) : pool(nbrOfThreads), is_running(false){
	std::cout << "Threads in pool: " << nbrOfThreads << std::endl;
}

void ThreadPool::work() {
	while (is_running) {
		if (!jobs.empty()) {
			std::function<void()> func;
			{
				std::lock_guard<std::mutex> guard(q_mutex);
				func = jobs.front();
				jobs.pop();
			}
			func();
		}else{
			std::unique_lock<std::mutex> lk(q_mutex);
			cv.wait(lk);
		}
	}
}

void ThreadPool::start() {
	is_running = true;
	for (uint i = 0; i < pool.size(); ++i) {
		pool[i] = std::thread(&ThreadPool::work, this);
	}
}

void ThreadPool::shutdown() {
	is_running = false;
	cv.notify_all();
	for (uint i = 0; i < pool.size(); ++i) {
		pool[i].join();
	}
}

template<typename TF, class ... Args>
auto ThreadPool::enqueue(TF&& f, Args&&... args) -> std::future<decltype(f(args...))> { // @suppress("Member declaration not found")
	using return_type = decltype(f(args...));

	std::function<return_type()> func = std::bind(std::forward<TF>(f), std::forward<Args>(args)...);

	auto task = std::make_shared<std::packaged_task<return_type()>>(func);

	std::function<void()> wrapper_task = [task]() {
		(*task)();
	};

	{
		std::lock_guard<std::mutex> guard(q_mutex);
		jobs.push(wrapper_task);
	}
	cv.notify_one();

	std::future<return_type> fut = task->get_future();

	return fut;
}

#endif
