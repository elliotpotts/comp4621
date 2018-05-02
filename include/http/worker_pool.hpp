#ifndef COMP4621_WORKER_POOL_HPP_INCLUDED
#define COMP4621_WORKER_POOL_HPP_INCLUDED
#include <vector>
#include <thread>
#include <condition_variable>
#include <deque>
#include <future>
#include <mutex>
#include <atomic>

namespace http {
    template<typename T>
    class worker_pool {
        std::vector<T> workers;
        std::vector<std::thread> threads;
        
        // Tasks
        std::deque<std::packaged_task<void(T&)>> tasks;
        std::condition_variable task_available;
        std::mutex task_mutex;
        
        // Finished event
        std::atomic<int> tasks_left;
        std::condition_variable finished;
        std::mutex finish_mutex;
        
        public:
        worker_pool(int n_threads) : workers(n_threads) {
            threads.reserve(n_threads);
            for(int i = 0; i < n_threads; i++) {
                threads.emplace_back([i, this](){
                    while(true) {
                        std::packaged_task<void(T&)> current_task;
                        {
                            std::unique_lock<std::mutex> lock(task_mutex);
                            task_available.wait(lock, [&](){ return tasks.size() > 0;});
                            current_task = std::move(tasks.front());
                            tasks.pop_front();
                        }
                        current_task(workers[i]);
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

        ~worker_pool() {
            if(tasks.size() > 0) finish_all();
            for(auto& t : threads) {
                t.detach();
            }
        }
        
        void finish_all() {
            std::unique_lock<std::mutex> lock(finish_mutex);
            finished.wait(lock, [&](){ return tasks.size() == 0;});
        }

        template<typename... ArgTs>
        void post_task(ArgTs&&... args) {
            std::lock_guard<std::mutex> lock(task_mutex);
            tasks.emplace_back([args = std::make_tuple(std::forward<ArgTs>(args)...)](T& worker){
                std::apply(worker, std::move(args));
            });
            tasks_left++;
            task_available.notify_one();
        }
    };
}
#endif