#include "epoller.h"


Epoller::Epoller(int maxEvent):epollFd_(epoll_create(512)), events_(maxEvent ){
    assert(epollFd_>=0 && events_.size()>0);
}


bool Epoller::AddFd(int fd, uint32_t events){
    if(fd<0) return false;
    struct epoll_event event={0};
    event.data.fd = fd;
    event.events = events;

    if(epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &event)==-1) return false;
    return true; 

}

bool Epoller::ModFd(int fd, uint32_t events){
    if(fd<0) return false;
    struct epoll_event event={0};
    event.data.fd = fd;
    event.events = events;
    return epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &event)==0;

}

bool Epoller::DelFd(int fd){
    if(fd<0) return false;
    struct epoll_event event={0};
    return epoll_ctl(epollFd_,EPOLL_CTL_DEL,fd,&event)==0;
}


int Epoller::Wait(int timeoutMs){
    return epoll_wait(epollFd_, &events_[0],static_cast<int>(events_.size()),timeoutMs);
}

int Epoller::GetEventFd(size_t i) const {
    assert(i < events_.size() && i >= 0);
    return events_[i].data.fd;
}

uint32_t Epoller::GetEvents(size_t i) const {
    assert(i < events_.size() && i >= 0);
    return events_[i].events;
}

Epoller::~Epoller() {
    close(epollFd_);
}