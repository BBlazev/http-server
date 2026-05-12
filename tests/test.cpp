#include "ThreadPool.hpp"
#include <iostream>
#include <atomic>
#include <chrono>
#include <cassert>
#include <mutex>

std::mutex cout_mu;
void safe_print(const std::string& msg) {
    std::lock_guard<std::mutex> lock(cout_mu);
    std::cout << msg;
}


void test_1000_increments() {
    std::cout << "\n[TEST 1] 1000 Increments\n";
    std::atomic<int> counter{0};
    
    {
        ThreadPool pool(4);
        for (int i = 0; i < 1000; ++i) {
            pool.submit([&counter]() {
                counter++;
            });
        }
    } 

    std::cout << "Final counter value: " << counter.load() << "\n";
    assert(counter == 1000 && "Counter should exactly equal 1000");
    std::cout << "-> PASSED\n";
}


void test_parallelism() {
    std::cout << "\n[TEST 2] Parallelism (100 tasks, 50ms sleep, 8 workers)\n";
    
    auto start = std::chrono::steady_clock::now();
    {
        ThreadPool pool(8);
        for (int i = 0; i < 100; ++i) {
            pool.submit([i]() {
                {
                    std::lock_guard<std::mutex> lock(cout_mu);
                    std::cout << "Task " << i << " running on thread: " 
                              << std::this_thread::get_id() << "\n";
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            });
        }
    }
    auto end = std::chrono::steady_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::cout << "Total execution time: " << elapsed_ms << "ms\n";
    
    assert(elapsed_ms < 1500 && "Execution took too long, parallelism failed!");
    std::cout << "-> PASSED\n";
}


void test_clean_shutdown() {
    std::cout << "\n[TEST 3] Clean Shutdown\n";
    std::atomic<int> completed_tasks{0};
    
    {
        ThreadPool pool(4);
        for (int i = 0; i < 50; ++i) {
            pool.submit([&completed_tasks]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                completed_tasks++;
            });
        }
        std::cout << "Tasks submitted. Destroying pool immediately...\n";
    } 

    std::cout << "Completed tasks after destruction: " << completed_tasks.load() << "\n";
    assert(completed_tasks == 50 && "Destructor did not wait for queued tasks to finish!");
    std::cout << "-> PASSED\n";
}


void test_empty_pool() {
    std::cout << "\n[TEST 4] Empty Pool\n";
    
    auto start = std::chrono::steady_clock::now();
    {
        ThreadPool pool(4);
    } // destructor should exit immediately
    auto end = std::chrono::steady_clock::now();
    
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    std::cout << "Creation and destruction time: " << elapsed_ms << "ms\n";
    assert(elapsed_ms < 50 && "Empty pool destruction hung or took too long!");
    std::cout << "-> PASSED\n";
}


int main() {
    std::cout << "Starting ThreadPool tests...\n";
    std::cout << "============================\n";
    
    test_1000_increments();
    test_parallelism();
    test_clean_shutdown();
    test_empty_pool();
    
    std::cout << "\n============================\n";
    std::cout << "All ThreadPool tests passed successfully!\n";
    
    return 0;
}