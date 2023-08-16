#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <thread>
#include <mutex>
#include <atomic>
#include <queue>
#include <functional>
#include "threadsafe_queue.h"

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

template<typename T>
class thread_pool
{
public:
    thread_pool();
    ~thread_pool();
    static void* worker_thread(void *arg);
    void run();
    void submit(T *f)  ;
private:
    std::atomic_bool done;
    join_threads joiner;
    threadsafe_queue<T> work_queue;
    std::vector<std::thread> threads;
};

template<typename T>
thread_pool<T>::thread_pool():done(false),joiner(threads)
{
    unsigned const nums=std::thread::hardware_concurrency();
    try {
        for(unsigned i=0;i<nums;++i){
            threads.emplace_back(std::thread(&thread_pool::worker_thread,this));
        }
    }  catch (...) {
        done=true;
        throw;
    }
}

template<typename T>
void thread_pool<T>::submit(T *f)
{
      work_queue.push(f);
}
template<typename T>
thread_pool<T>::~thread_pool()
{
    done=true;
}

template<typename T>
void thread_pool<T>::run()
{
    while(!done){
        T task;
        if(work_queue.try_pop(task))
        {

            task.process();

        }
        else
        {
             std::this_thread::yield();//等待
        }
    }
}

template<typename T>
void* thread_pool<T>::worker_thread(void *arg)
{
    thread_pool *pool=(thread_pool *) arg;
    pool->run();
    return pool;

}

#endif // THREAD_POOL_H
