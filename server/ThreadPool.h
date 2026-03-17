#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <thread>
#include <mutex>
#include <queue>
#include <functional>
#include <vector>
#include <condition_variable>

class ThreadPool {
public:
    ThreadPool(size_t thread_count = 4);
    ~ThreadPool();

    // 添加任务到队列
    void enqueue(std::function<void()> task);

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
};

#endif // THREADPOOL_H
