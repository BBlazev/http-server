#pragma once

#include <functional>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <queue>
#include <vector>

class ThreadPool
{
public:

    explicit ThreadPool(size_t num_workers);
    ~ThreadPool();

    ThreadPool() = delete; // for clarity, compiler wont auto generate since we provided explicit construc above
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    void submit(std::function<void()> task);

private:

    std::mutex mu;
    std::condition_variable cv;
    bool shutdown = false;
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;

    void worker_loop();

};