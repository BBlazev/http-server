#include "ThreadPool.hpp"
#include <unistd.h>
#include <iostream>

ThreadPool::ThreadPool(size_t num_workers)
{
    for(size_t i = 0; i < num_workers; i++)
    {
        //workers.emplace_back(&ThreadPool::worker_loop, this);
        workers.emplace_back([this]{worker_loop();}); // cleaner intent than prev
    }
}

ThreadPool::~ThreadPool()
{
    {
        std::lock_guard<std::mutex> lock(mu);
        shutdown = true;
    }
    cv.notify_all();
    
    for (auto& w : workers) 
        if (w.joinable()) w.join();
    
}

void ThreadPool::submit(std::function<void()> task)
{
    {
        std::lock_guard<std::mutex> guard(mu);
        tasks.push(std::move(task));
    }
    cv.notify_one();
}

void ThreadPool::worker_loop()
{
    while (true)
    {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> guard(mu);
            

            cv.wait(guard, [this]
            {
                return shutdown || !tasks.empty();
            });

            if(shutdown && tasks.empty()) return;

            

            task = std::move(tasks.front());
            tasks.pop();
        }
        try
        {
            task();
        }
        catch(const std::exception& e)
        {
            std::cerr << e.what() << '\n';
        }
        
    }
    
}
