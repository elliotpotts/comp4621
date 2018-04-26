#ifndef COMP4621_THREADPOOL_HPP_INCLUDED
#define COMP4621_THREADPOOL_HPP_INCLUDED
#include <vector>
#include <thread>
#include <condition_variable>
#include <deque>
#include <future>
#include <mutex>
#include <atomic>
namespace http {
    class threadpool {
        std::vector<std::thread> workers;
        
        // Tasks
        std::deque<std::packaged_task<void()>> tasks;
        std::condition_variable task_available;
        std::mutex task_mutex;
        
        // Finished event
        std::atomic<int> tasks_left;
        std::condition_variable finished;
        std::mutex finish_mutex;
        
        public:
        threadpool(int n_threads) {
            workers.reserve(n_threads);
            for(int i = 0; i < n_threads; i++) {
                workers.emplace_back([&](){
                    while(true) {
                        std::packaged_task<void()> current_task;
                        {
                            std::unique_lock<std::mutex> lock(task_mutex);
                            task_available.wait(lock, [&](){ return tasks.size() > 0;});
                            current_task = std::move(tasks.front());
                            tasks.pop_front();
                        }
                        current_task();
                        tasks_left--;
                        if(tasks_left.load() == 0) {
                            std::lock_guard<std::mutex> lock(finish_mutex);
                            finished.notify_one();
                        }
                    }
                });
            }
            tasks_left.store(0);
        }

        ~threadpool() {
            if(tasks.size() > 0) finish_all();
            for(auto& w : workers) {
                w.detach();
            }
        }
        
        void finish_all() {
            std::unique_lock<std::mutex> lock(finish_mutex);
            finished.wait(lock, [&](){ return tasks.size() == 0;});
        }

        template<typename F>
        void post_task(F&& f) {
            std::lock_guard<std::mutex> lock(task_mutex);
            tasks.emplace_back(std::forward<F>(f));
            tasks_left++;
            task_available.notify_one();
        }
    };
}
#endif