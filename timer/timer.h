#ifndef HEAPTIMER_H
#define HEAPTIMER_H

#include <queue>
#include <unordered_map>
#include <time.h>
#include <algorithm>
#include <arpa/inet.h>
#include <functional>
#include <assert.h>
#include <chrono>

#include "../log/log.h"

typedef std::function<void()> TimeOutBack;
//高精度时钟
//当前系统能够提供的最高精度的时钟；它也是不可以修改的。相当于 steady_clock 的高精度版本。是c++11高精度时钟；主要是使用它的一个now()方法；
//（使用auto可以接受任何类型，称”自动类型“）；精确到纳秒
typedef std:: chrono::high_resolution_clock Clock;
//毫秒
typedef std::chrono::milliseconds MS;
// 表示时间中的一个点
//时间辍
typedef Clock::time_point TimeStamp;

struct TimerNode
{
    int id;//计时器的唯一标识符
    TimeStamp expires;//超时时间戳
    TimeOutBack cb;//超时后调用的回调函数 
    bool operator<(const TimerNode& t){
        //断当前对象的 expires 是否小于参数对象 t 的 expires，
        //如果条件成立，就返回 true，否则返回 false
        return expires<t.expires;
        //例如，如果当前有两个定时器，A 和 B，其中 A 的超时时间较早，那么 A < B 的比较应该返回 true，这使得 A 可以在调度时被优先处理。
        //当你需要取出最早到期的定时器时，这个比较运算符非常有用
    }
};

class HeapTimer{
public:
    HeapTimer() {heap_.reserve(64);}
    ~HeapTimer() {clear();}
    void adjust(int id,int newExpires);
    void add(int id,int timeout,const TimeOutBack& cb);
    void dowork(int id);
    void clear();
    void tick();
    void pop();
    int GetNextTick();

private:
    std::vector<TimerNode> heap_;
    std::unordered_map<int,size_t> ref;

    void del_(size_t i);
    void siftup_(size_t i);//向上调整
    bool siftdown_(size_t index,size_t n);//向下调整
    void SwapNode_(size_t i,size_t j);
};




#endif
