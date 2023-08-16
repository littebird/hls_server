#include "thread_pool.h"


thread_pool::thread_pool():done(false),joiner(threads)
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

thread_pool::~thread_pool()
{
    done=true;
}

void thread_pool::worker_thread()
{
    while(!done){
        std::function<void(Connection *)>task(Connection *conn_data);

        if(work_queue.try_pop(task))
        {

            Connection *data=new Connection();
            task(data);

        }

        else
        {
             std::this_thread::yield();//等待

        }

    }

}

