#ifndef ADAPTIVE_THREADPOOL_H
#define ADAPTIVE_THREADPOOL_H
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <algorithm> 
#include <assert.h>
#include <math.h>
#include <iostream>
#include <unistd.h>

class ThreadPool {
public:
    friend class AdaptiveThreadPool;
    explicit ThreadPool(size_t thread_count)
        : stop(false)
    {
        for (size_t i = 0; i < thread_count; ++i) {
            workers.emplace_back(
                [this] {
                    for (;;) {
                        std::function<void()> task;

                        {
                            std::unique_lock<std::mutex> lock(this->queue_mutex);
                            this->condition.wait(lock,
                                [this] { return this->stop || !this->tasks.empty(); });
                            if (this->stop && this->tasks.empty())
                                return;
                            task = std::move(this->tasks.front());
                            this->tasks.pop();
                        }

                        task();
                    }
                }
            );
        }
    }
    ThreadPool() = default;
    ThreadPool(ThreadPool&&) = default;
    
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type>
    {
        using return_type = typename std::result_of<F(Args...)>::type;

        auto task = std::make_shared<std::packaged_task<return_type()> >(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<return_type> res = task->get_future();  // 如果任务有返回值的话，可以通过这个进行获取
        {
            std::unique_lock<std::mutex> lock(queue_mutex);

            // Don't allow enqueueing after stopping the pool
            if (stop)
                throw std::runtime_error("enqueue on stopped ThreadPool");

            tasks.emplace([task]() { (*task)(); });
        }
        condition.notify_one();
        return res;
    }

    ~ThreadPool()
    {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        condition.notify_all();
        for (std::thread& worker : workers){
            worker.join();
            std::cout<<"th析构"<<std::endl;
        }
            
    }

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()> > tasks;

    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
};

class AdaptiveThreadPool {
public:
    AdaptiveThreadPool(size_t min_threads, size_t max_threads, double load_factor)
        : pool(min_threads)
        , min_threads(min_threads)
        , max_threads(max_threads)
        , load_factor(load_factor)
        , total_tasks(0)
        , stop(false)
    {

        check_interval = std::chrono::seconds(5);
        check_thread = std::thread(&AdaptiveThreadPool::check_load, this);
    }

    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type>
    {   
        
        return pool.enqueue(std::forward<F>(f), std::forward<Args>(args)...);
    }

    ~AdaptiveThreadPool()
    {
        // 程序结束时析构，因此只要任务队列为空，则可以析构
        while(1){
            {
                std::unique_lock<std::mutex> lock(pool.queue_mutex);
                if(pool.tasks.size()==0){
                    stop=true;
                    break;
                }
            }
            sleep(3);
        }
        check_thread.join();
        std::cout<<"adth析构"<<std::endl;
    }

private:
    ThreadPool pool;
    size_t min_threads;
    size_t max_threads;
    double load_factor;
    size_t total_tasks;
    std::chrono::seconds check_interval;
    std::thread check_thread;
    bool stop;
    void check_load()
    {
        for (;;) {
            if(stop==true) return;
            std::this_thread::sleep_for(check_interval);

            size_t num_threads = pool.workers.size();
            size_t active_threads = 0;
            for (size_t i = 0; i < num_threads; ++i) {
                std::thread::id tid = pool.workers[i].get_id();
                if (std::find_if(std::begin(pool.workers), std::end(pool.workers),
                    [&](const std::thread& t) { return t.get_id() == tid && t.joinable(); }) != std::end(pool.workers)) {
                    ++active_threads;
                }
            }
            assert(active_threads>0);
            {
                std::unique_lock<std::mutex> lock(pool.queue_mutex);
                total_tasks = pool.tasks.size();
            }
            double load = static_cast<double>(total_tasks) / active_threads;
            // 加入新的线程
            if (load > load_factor && num_threads < max_threads) {
                pool.workers.emplace_back(
                    [this] {
                        for (;;) {
                            std::function<void()> task;

                            {
                                std::unique_lock<std::mutex> lock(this->pool.queue_mutex);
                                this->pool.condition.wait(lock,[this] { return this->pool.stop || !this->pool.tasks.empty(); });
                                if (this->pool.stop && this->pool.tasks.empty())
                                    return;
                                task = std::move(this->pool.tasks.front());
                                this->pool.tasks.pop();
                            }

                            task();
                        }
                    }
                );
            }
            // 释放部分线程
            else if (load < load_factor / 2 && num_threads > min_threads) {
                if(num_threads==min_threads) continue;
                
                std::unique_lock<std::mutex> lock(pool.queue_mutex);
                pool.stop = true;
                pool.condition.notify_all();
                lock.unlock();

                for (std::thread& worker : pool.workers) {
                    worker.join();
                }
                pool.workers.clear();
                pool.stop = false;

                size_t new_num_threads = std::max(num_threads / 3, min_threads);
                for (size_t i = 0; i < new_num_threads; ++i) {
                    pool.workers.emplace_back(
                        [this] {
                            for (;;) {
                                std::function<void()> task;

                                {
                                    std::unique_lock<std::mutex> lock(this->pool.queue_mutex);
                                    this->pool.condition.wait(lock,
                                        [this] { return this->pool.stop || !this->pool.tasks.empty(); });
                                    if (this->pool.stop && this->pool.tasks.empty())
                                        return;
                                    task = std::move(this->pool.tasks.front());
                                    this->pool.tasks.pop();
                                }

                                task();
                            }
                        }
                    );
                }
            }
        }
    }

};





#endif