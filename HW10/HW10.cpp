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
	 * m_threadCount - кількість потоків у pool(number of threads in the pool)
	 * threads - вектор для утримання робочих потоків(vector to hold worker threads)
	 * tasks - черга для зберігання завдань(queue to store tasks)
	 * condition - умовна змінна для синхронізації завдань(condition variable for task synchronization)
	 * mut - mutex для захисту потоків (Mutex for thread safety)
	 * stop - прапорець для сигналізації зупинки потоку(Flag to signal threads to stop)
	 */
	size_t m_threadCount; 
    std::vector<std::thread> threads;
    std::queue<std::function<void()>> tasks;
    std::condition_variable condition;
    std::mutex mut;
    bool stop;

public:
	/**
	 * \brief Конструктор для ініціалізації pool'y потоків з заданою кількістю потоків
	 * \param threadCount - кількість потоків, які необхідно ініціалізувати.
	 *  (threadCount - the number of threads to be initialized.)
	 */
	explicit SimpleThreadPool(std::size_t threadCount) : m_threadCount(threadCount), stop(false)
	{
        /**
         * Створюємо робочі потоки та зв'язати їх з методом WorkOn
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
	 *  Метод розміщення завдання в pool потоків
	  * (Method to post a task to the thread pool)
	 */
	template <typename Fnc_T>
    auto Post(Fnc_T task) -> std::future<decltype(task())>
	{
        /**
         * Обгорнемо задачу в упаковану задачу для асинхронного виконання.
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
         * Повернути future до результату завдання
         *(Return a future to the result of the task)
         */
        return wrapper->get_future();
    }

	/**
	 * Метод для виконання завдань
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
                 * Очікуємо, до поки не з'явиться завдання або не буде зупинено pool потоків.
                 * (Wait until there's a task available or the thread pool is stopped)
                 */
                condition.wait(lock, [&] { return stop || !tasks.empty(); });
                /**
                 *  Якщо pool потоків зупинено і задачі відсутні - виходимо з циклу.
                 *
                 */
                if (stop && tasks.empty()) 
                {
                    return;
                }
                /**
                 * (Отримаємо завдання з черги)
                 * 
                 */
                task = std::move(tasks.front());
                tasks.pop();
            }
            task();
        }
    }

	/**
	 * Метод знищення pool'y потоків
	 * (Method to destroy the thread pool)
	 */
	void Destroy()
	{
        {
            std::unique_lock<std::mutex> lock(mut);
            stop = true;
        }
        /**
         * Сповістити всі потоки, що знаходяться у стані очікування, що pool потоків буде знищено.
         * (Notify all waiting threads that the thread pool is being destroyed)
         */
        condition.notify_all();
        /**
         * Приєднуємо всі робочі потоки, щоб переконатися, що вони завершили виконувати завдання
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
     * Обчислюємо факторіал числа
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
     * Розміщуємо завдання у thread pool
     * (Posting tasks to the thread pool)
     */
    std::vector<std::future<long long>> futures;
    for (long long ind = 1; ind <= numberForFactorial; ++ind) 
    {
        futures.push_back(pool.Post(std::bind(factorialTask, ind)));
    }

    /**
     * Отримуємо результати із вектора futures
     * (Getting results from futures vector)
     */
    for (auto& future : futures) 
    {
        std::cout << "Factorial result: " << future.get() << std::endl;
    }

    return 0;
}
