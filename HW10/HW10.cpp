#include <iostream>
#include <thread>
#include <vector>
#include <queue>
#include <functional>
#include <future>
#include <mutex>
#include <condition_variable>

class SimpleThreadPool {
    
private:
	/**
	 * m_threadCount - ������� ������ � pool(number of threads in the pool)
	 * threads - ������ ��� ��������� ������� ������(vector to hold worker threads)
	 * tasks - ����� ��� ��������� �������(queue to store tasks)
	 * condition - ������ ����� ��� ������������ �������(condition variable for task synchronization)
	 * mut - mutex ��� ������� ������ (Mutex for thread safety)
	 * stop - ��������� ��� ����������� ������� ������(Flag to signal threads to stop)
	 */
	size_t m_threadCount; 
    std::vector<std::thread> threads;
    std::queue<std::function<void()>> tasks;
    std::condition_variable condition;
    std::mutex mut;
    bool stop;

public:
	/**
	 * \brief ����������� ��� ����������� pool'y ������ � ������� ������� ������
	 * \param threadCount - ������� ������, �� ��������� ������������.
	 *  (threadCount - the number of threads to be initialized.)
	 */
	explicit SimpleThreadPool(std::size_t threadCount) : m_threadCount(threadCount), stop(false)
	{
        /**
         * ��������� ������ ������ �� ��'����� �� � ������� WorkOn
         * (Create worker threads and bind them to the WorkOn method)
         */
        for (std::size_t i = 0; i < m_threadCount; ++i) {
            threads.emplace_back(std::bind(&SimpleThreadPool::WorkOn, this));
        }
    }

    ~SimpleThreadPool()
	{
        Destroy();
    }

    SimpleThreadPool(const SimpleThreadPool&) = delete;
    SimpleThreadPool& operator=(const SimpleThreadPool&) = delete;

	/**
	 *  ����� ��������� �������� � pool ������
	  * (Method to post a task to the thread pool)
	 */
	template <typename Fnc_T>
    auto Post(Fnc_T task) -> std::future<decltype(task())>
	{
        /**
         * ��������� ������ � ��������� ������ ��� ������������ ���������.
         * (Wrap the task into a packaged task to be executed asynchronously)
         */
        auto wrapper = std::make_shared<std::packaged_task<decltype(task())()>>(std::move(task));
        {
            std::unique_lock<std::mutex> lock(mut);
            tasks.emplace([=]() 
            {
                (*wrapper)();
            });
        }
        condition.notify_one();
        /**
         * ��������� future �� ���������� ��������
         *(Return a future to the result of the task)
         */
        return wrapper->get_future();
    }

	/**
	 * ����� ��� ��������� �������
	 * (Method to execute tasks)
	 */
	void WorkOn()
	{
        while (true) 
        {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(mut);
                /**
                 * �������, �� ���� �� �'������� �������� ��� �� ���� �������� pool ������.
                 * (Wait until there's a task available or the thread pool is stopped)
                 */
                condition.wait(lock, [&] { return stop || !tasks.empty(); });
                /**
                 *  ���� pool ������ �������� � ������ ������ - �������� � �����.
                 *
                 */
                if (stop && tasks.empty()) 
                {
                    return;
                }
                /**
                 * (�������� �������� � �����)
                 * 
                 */
                task = std::move(tasks.front());
                tasks.pop();
            }
            task();
        }
    }

	/**
	 * ����� �������� pool'y ������
	 * (Method to destroy the thread pool)
	 */
	void Destroy()
	{
        {
            std::unique_lock<std::mutex> lock(mut);
            stop = true;
        }
        /**
         * ��������� �� ������, �� ����������� � ���� ����������, �� pool ������ ���� �������.
         * (Notify all waiting threads that the thread pool is being destroyed)
         */
        condition.notify_all();
        /**
         * �������� �� ������ ������, ��� ������������, �� ���� ��������� ���������� ��������
         * (Join all worker threads to ensure they finish executing tasks)
         */
        for (std::thread& thread : threads) 
        {
            thread.join();
        }
    }
};

int main() {
    SimpleThreadPool pool(4);
    /**
     * ���������� �������� �����
     * (Calculating factorial of a number)
     */
    auto factorialTask = [](long long n)
	{
        long long result = 1;
        for (long long ind = 1; ind <= n; ++ind) 
        {
            result *= ind;
        }
        return result;
    };
    constexpr int numberForFactorial = 20;
    /*
     * �������� �������� � thread pool
     * (Posting tasks to the thread pool)
     */
    std::vector<std::future<long long>> futures;
    for (long long ind = 1; ind <= numberForFactorial; ++ind) 
    {
        futures.push_back(pool.Post(std::bind(factorialTask, ind)));
    }

    /**
     * �������� ���������� �� ������� futures
     * (Getting results from futures vector)
     */
    for (auto& future : futures) 
    {
        std::cout << "Factorial result: " << future.get() << std::endl;
    }

    return 0;
}
