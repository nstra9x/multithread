#include <iostream>
#include <vector>
#include <thread>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <future>
#include <chrono>

class ThreadPool {
public:
    ThreadPool(size_t threads);
    ~ThreadPool();

    // Hàm enqueue đơn giản, chỉ nhận công việc không có tham số và không trả về giá trị
    std::future<void> enqueue(std::function<void()> task);

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;

    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
};

// Khởi tạo ThreadPool với số lượng luồng nhất định
ThreadPool::ThreadPool(size_t threads) : stop(false) {
    for (size_t i = 0; i < threads; ++i) {
        workers.emplace_back([this] {
            for (;;) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(this->queue_mutex);
                    this->condition.wait(lock, [this] { return this->stop || !this->tasks.empty(); });

                    if (this->stop && this->tasks.empty())
                        return;

                    task = std::move(this->tasks.front());
                    this->tasks.pop();
                }

                task();
            }
            });
    }
}

// Hàm enqueue để thêm công việc vào hàng đợi
std::future<void> ThreadPool::enqueue(std::function<void()> task) {
    auto task_wrapper = std::make_shared<std::packaged_task<void()>>(task);
    std::future<void> res = task_wrapper->get_future();

    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        if (stop)
            throw std::runtime_error("enqueue on stopped ThreadPool");

        tasks.emplace([task_wrapper]() { (*task_wrapper)(); });
    }
    condition.notify_one();
    return res;
}

// Hàm hủy ThreadPool và dừng tất cả các luồng
ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;
    }
    condition.notify_all();
    for (std::thread& worker : workers)
        worker.join();
}

int main() {
    ThreadPool pool(2); // Khởi tạo thread pool với 3 luồng

    // Thêm các công việc vào pool với độ trễ để minh họa sự đồng thời
    auto task0 = pool.enqueue([]() {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        std::cout << "Task 0 completed\n";
        });

    auto task1 = pool.enqueue([]() {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        std::cout << "Task 1 completed\n";
        });

    auto task2 = pool.enqueue([]() {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << "Task 2 completed\n";
        });

    auto task3 = pool.enqueue([]() {
        std::this_thread::sleep_for(std::chrono::seconds(3));
        std::cout << "Task 3 completed\n";
        });

    // Đợi các công việc hoàn thành
    task1.get();
    task2.get();
    task3.get();

    std::cout << "All tasks completed\n";
    return 0;
}
