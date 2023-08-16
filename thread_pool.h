#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <thread>
#include <mutex>
#include <atomic>
#include <queue>
#include <functional>
#include "threadsafe_queue.h"
#include"connection.h"
class join_threads
{
public:
    explicit join_threads(std::vector<std::thread>& threads):_threads(threads){}
    ~join_threads(){
        for (unsigned long i=0; i<_threads.size(); ++i) {
            if(_threads[i].joinable())
                _threads[i].join();//等待线程结束
        }
    }
private:
    std::vector<std::thread>& _threads;

};

class thread_pool
{
public:
    thread_pool();
    ~thread_pool();
    void worker_thread();
    template<typename func>
    void submit(func f(Connection *conn_data)){//函数模板，提交一个任务到任务队列中
        work_queue.push(std::function<void(Connection *)>(f));
    }
private:
    std::atomic_bool done;
    join_threads joiner;
    threadsafe_queue<std::function<void(Connection *)>> work_queue;
    std::vector<std::thread> threads;
};

#endif // THREAD_POOL_H
