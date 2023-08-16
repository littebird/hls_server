#ifndef THREADSAFE_QUEUE_H
#define THREADSAFE_QUEUE_H

#include <queue>
#include <mutex>
#include <condition_variable>
#include<iostream>
template<typename T>
class threadsafe_queue
{
public:
    threadsafe_queue(){}
    bool try_pop(T& value){
        std::unique_lock<std::mutex> lk(mt);
        if(data_queue.empty())
            return false;
        value=std::move(*data_queue.front());
        data_queue.pop();
        return true;
    }

    void push(T newVal){

        std::shared_ptr<T>data(std::make_shared<T>(std::move(newVal)));

        std::unique_lock<std::mutex> lk(mt);
        data_queue.push(data);
        data_cond.notify_one();//唤醒一个线程处理
    }
    bool empty()const{
        std::unique_lock<std::mutex>lk(mt);
        return data_queue.empty();
    }


private:
    mutable std::mutex mt;
    std::queue<std::shared_ptr<T>>data_queue;
    std::condition_variable data_cond;

};
#endif // THREADSAFE_QUEUE_H
