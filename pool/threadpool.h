#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <mutex>
//C++11多线程条件变量，C++ 11新特性， 包含多线程中常用的条件变量的声明，例如notify_one、wait、wait_for等等
#include <condition_variable>
#include <queue>
#include <thread>
#include <assert.h>
//C++ 11增加了一些新特性，简单来说可以实现函数到对象的绑定，如bind()函数。
#include <functional>

class ThreadPool{
public:
    explicit ThreadPool(size_t threadCount=8):pool_(std::make_shared<Pool>()){
        assert(threadCount>0);
        for(size_t i=0;i<threadCount;i++){
            //线程的行为由 lambda 表达式定义。这个 lambda 表达式捕获了 pool_ 的指针（或者智能指针）
            //以便它可以访问线程池的资源。
            std::thread([pool=pool_]{
                std::unique_lock<std::mutex> locker(pool->mtx);
                while(true){
                    if(!pool->tasks.empty()){
                        auto task=std::move(pool->tasks.front());
                        pool->tasks.pop();
                        locker.unlock();
                        task();
                        locker.lock();
                    }   
                    else if(pool->isClosed) break;
                    else pool->cond.wait(locker);
                }
            }).detach();
        }
    }

    //告诉编译器生成一个简单的构造函数，这个构造函数不执行任何特定初始化，因此适用于当类中没有复杂的资源分配时。
    ThreadPool()=default;

    //告诉编译器生成一个默认的的移动构造函数
    //移动构造函数用于将一个对象的资源（例如动态分配的内存、文件句柄等）
    //“移动”到另一个对象中，而不是进行深拷贝。这样可以显著提高性能，尤其是在对象内容较大时
    ThreadPool(ThreadPool&&)=default;

    ~ThreadPool(){
        if(static_cast<bool>(pool_)){
            //lock_guard类是一个常见的方式来管理互斥量（mutex）以确保线程安全
            std::lock_guard<std::mutex> locker(pool_->mtx);
            pool_->isClosed=true;
        }
        //通知等待在该条件变量上的所有线程。
        //这通常用于线程间的同步，特别是在某些条件满足时（例如，一个资源变得可用），让所有等待的线程被唤醒。
        pool_->cond.notify_all();
        //nodify_one()是唤醒一个线程
    }

    //F 可以是任何可调用的类型
    template<class F>
    void AddTask(F&& task){
        std::lock_guard<std::mutex> locker(pool_->mtx);
        //保持 task 对原始参数类型的引用性质。
        //这样，无论是以值传递、左值引用还是右值引用方式传入函数，std::forward 将确保其被适当地转发到 emplace 方法。
        pool_->tasks.emplace(std::forward<F>(task));
    }


private:
    struct Pool
    {
        std::mutex mtx;
        std::condition_variable cond;
        bool isClosed;
        //创建人物队列存放任务，存放的是没有返回值的void函数
        std::queue<std::function<void()>> tasks;
    };
    std::shared_ptr<Pool> pool_;
    
};



#endif
