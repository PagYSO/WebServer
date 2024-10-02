#include "epoller.h"

Epoller::Epoller(int maxEvent):epollFd(epoll_create(512)),events(maxEvent){
    assert(epollFd >=0 && events.size()>0);
}

Epoller::~Epoller(){
    close(epollFd);
}

bool Epoller::AddFd(int fd,uint32_t events){
    if(fd<0) return false;
    epoll_event ev={0};
    ev.data.fd=fd;
    ev.events=events;

    //epoll_ctl:注册监控事件 EPOLL_CTL_ADD 注册新的fd到epfd中
    return 0==epoll_ctl(epollFd,EPOLL_CTL_ADD,fd,&ev);
    //成功返回0，不成功返回-1并设置errno

}

bool Epoller::ModFd(int fd,uint32_t events){
    if(fd<0) return false;
    epoll_event ev={0};
    ev.data.fd=fd;
    ev.events=events;
    return 0==epoll_ctl(epollFd,EPOLL_CTL_MOD,fd,&ev);
    
}

bool Epoller::DelFd(int fd){
    if(fd<0) return false;
    epoll_event ev={0};
    return 0==epoll_ctl(epollFd,EPOLL_CTL_DEL,fd,&ev);
}

int Epoller::Wait(int timeoutMs){
    //static_cast 强转，相比(int)3.14拥有可读性，只能执行明确允许的转换，这有助于避免一些错误。
    return epoll_wait(epollFd,&events[0],static_cast<int>(events.size()),timeoutMs);
}

int Epoller::GetEventFd(size_t i) const{
    assert(events.size()>i && i>=0);
    return events[i].data.fd;
}

uint32_t Epoller::GetEvents(size_t i) const{
    assert(events.size()>i && i>=0);
    return events[i].events;
}

