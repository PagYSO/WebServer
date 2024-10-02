#include "heaptimer.h"
using namespace std;

void HeapTimer::siftup_(size_t i){
    assert(i>0 && i<heap_.size());
    size_t j=(i-1)/2;
    while(j>0){
        if(heap_[j]<heap_[i]) break;
        SwapNode_(i,j);
        i=j;
        j=(i-1)/2;
    }
}

void HeapTimer::SwapNode_(size_t i,size_t j){
    assert(i>0 && i<heap_.size());
    assert(j>0 && j<heap_.size());
    swap(heap_[i],heap_[j]);
    //更新节点的索引
    ref[heap_[i].id]=i;
    ref[heap_[j].id]=j;
    
}

bool HeapTimer::siftdown_(size_t index,size_t n){
    assert(index>0 && index<heap_.size());
    assert(n>0 && n<=heap_.size());
    size_t i=index;
    size_t j=i*2+1;
    while(j<n){
        if(j+1<n && heap_[j+1]<heap_[j]) j++;
        if(heap_[i]<heap_[j]) break;
        SwapNode_(i,j);
        i=j;
        j=i*2+1;
    }
    return i>index;
}

void HeapTimer::add(int id,int timeout,const TimeOutBack& cb){
    assert(id>=0);
    size_t i;
    if(ref.count(id)==0){
        i=heap_.size();
        ref[id]=i;
        //将新节点插入堆，节点包含 id、超时时间（当前时间加上超时值）和回调函数
        heap_.push_back({id,Clock::now()+MS(timeout),cb});
        siftup_(i);
    }
    else{
        i=ref[id];
        heap_[i].expires=Clock::now()+MS(timeout);
        heap_[i].cb=cb;
        if(!siftdown_(i,heap_.size())){
            siftup_(i);
        }
    }
}

void HeapTimer::del_(size_t index){
    assert(!heap_.empty() && index<heap_.size());
    size_t i=index;
    size_t n=heap_.size()-1;
    assert(i<=n);
    if(i<n){
        SwapNode_(i,n);
        if(!siftdown_(i,n)){
            siftup_(i);
        }
    }
    ref.erase(heap_.back().id);
    heap_.pop_back();
}

void HeapTimer::adjust(int id, int timeout) {
    /* 调整指定id的结点 */
    assert(!heap_.empty() && ref.count(id) > 0);
    heap_[ref[id]].expires = Clock::now() + MS(timeout);;
    siftdown_(ref[id], heap_.size());
}

void HeapTimer::tick() {
    // /更新定时器状态，例如检查和处理已过期的定时器
    /* 清除超时结点 */
    if(heap_.empty()) {
        return;
    }
    while(!heap_.empty()) {
        TimerNode node = heap_.front();
        if(std::chrono::duration_cast<MS>(node.expires - Clock::now()).count() > 0) { 
            break; 
        }
        node.cb();
        pop();
    }
}

void HeapTimer::pop() {
    assert(!heap_.empty());
    del_(0);
}

void HeapTimer::clear() {
    ref.clear();
    heap_.clear();
}

int HeapTimer::GetNextTick() {
    tick();
    size_t res = -1;
    if(!heap_.empty()) {
        //heap_.front 获取堆中到期时间最早的定时器的过期时间
        //计算当前时间与下一个到期时间之间的差值。
        //将这个时间差转换为毫秒
        res = std::chrono::duration_cast<MS>(heap_.front().expires - Clock::now()).count();
        //如果计算出的时间差 res 小于或等于 0，则将 res 设为 0。这意味着下一个定时器已经过期，
        //或者当前时间正好等于定时器的过期时间。
        if(res <= 0) { res = 0; }
    }
    return res;
}
