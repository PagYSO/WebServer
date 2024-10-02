#ifndef BLOCKQUEUE_H
#define BLOCKQUEUE_H

#include <mutex>
#include <deque>
#include <condition_variable>
#include <sys/time.h>
#include <assert.h>

template<class T>
class BlockDeque{
public:
    explicit BlockDeque(size_t MaxCapcity=1000);
    ~BlockDeque();

    void clear();

    bool empty();

    bool full();

    void Close();

    size_t size();

    size_t capacity();

    T front();

    T back();

    void push_back(const T &item);
    void push_front(const T &item);
    bool pop(T &item);
    bool pop(T &item,int timeout);
    void flush();
private:
    std::deque<T> deq_;
    size_t capacity_;
    bool isClose_;
    std::mutex mtx_;
    std::condition_variable condConsumer_;
    std::condition_variable condProducer_;
};


template<class T>
BlockDeque<T>::BlockDeque(size_t MaxCapacity): capacity_(MaxCapacity){
    assert(MaxCapacity>0);
    isClose_=false;
};

template<class T>
BlockDeque<T>::~BlockDeque(){
    Close();
};

template<class T>
void BlockDeque<T>::Close(){
    {
        std::lock_guard<std::mutex> locker(mtx_);
        deq_.clear();
        isClose_=true;
    }
    condConsumer_.notify_all();
    //会唤醒所有等待队列中阻塞的线程，存在锁争用，只有一个线程能够获得锁
    condProducer_.notify_all();
};

template<class T>
void BlockDeque<T>::flush(){
    condConsumer_.notify_one();
};

template<class T>
void BlockDeque<T>::clear(){
    std::lock_guard<std::mutex> locker(mtx_);
    deq_.clear();
};

template<class T>
T BlockDeque<T>::front(){
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.front();
};

template<class T>
T BlockDeque<T>::back(){
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.back();
};

template<class T>
size_t BlockDeque<T>::size(){
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.size();
};

template<class T>
size_t BlockDeque<T>::capacity(){
    std::lock_guard<std::mutex> locker(mtx_);
    return capacity_;
};

template<class T>
void BlockDeque<T>::push_back(const T &item){
    std::unique_lock<std::mutex> locker(mtx_);
    while(deq_.size()>=capacity_){
        condProducer_.wait(locker);
    }
    deq_.push_back(item);
    condConsumer_.notify_one();
};

template<class T>
void BlockDeque<T>::push_front(const T &item){
    std::unique_lock<std::mutex> locker(mtx_);
    if(deq_.size()>=capacity_){
        condProducer_.wait(locker);
    }
    deq_.push_front(item);
    condConsumer_.notify_one();
}

template<class T>
bool BlockDeque<T>::empty(){
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.empty();
};

template<class T>
bool BlockDeque<T>::full(){
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.size()>=capacity_;
}

template<class T>
bool BlockDeque<T>::pop(T &item,int timeout){
    std::unique_lock<std::mutex> locker(mtx_);
    while(deq_.empty()){
        //等待条件变量的通知，最多等待 timeout 秒
        if(condConsumer_.wait_for(locker,std::chrono::seconds(timeout))==std::cv_status::timeout){
                return false;
        //condConsumer_.wait(locker,std::chrono::seconds(timeout)
        //使当前线程在 locker 锁定的状态下等待
        //std::cv_status::timeout
        //表示等待超时，当前线程未被唤醒，代码会进入到超时处理逻辑
            }
        //如果 isClose_ 为 true，表示队列已关闭，返回 false
        if(isClose_){
            return false;
        }
    }
    item=deq_.front();
    deq_.pop_front();
    //在成功弹出元素后，通知一个等待的生产者线程，以便它可以继续生产
    condProducer_.notify_one();
    return true;
}

// std::unique_lock 提供了一种灵活的锁定机制，并且在调用 wait 时会自动释放锁，这样在等待条件变量的过程中，其他线程就可以获取到这个锁。
// 使用 std::lock_guard 时，锁会在作用域结束时自动释放，但它并不支持 wait，因为 wait 需要在释放锁的同时等待条件，再在条件满足后重新获取锁。


#endif
