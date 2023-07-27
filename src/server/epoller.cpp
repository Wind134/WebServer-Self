/*
eplooer的实现
*/

#include "epoller.h"

Epoller::Epoller(int maxEvent):epollFd_(epoll_create(512)), events_(maxEvent){
    assert(epollFd_ >= 0 && events_.size() > 0);    // 需要满足合理条件
}

Epoller::~Epoller() {
    close(epollFd_);    // 关闭epoll描述符
}

bool Epoller::AddFd(int fd, uint32_t events) {  // 添加文件描述符和感兴趣的事件类型(两者相互关联)
    if(fd < 0) return false;    // 不合理的文件描述符
    epoll_event ev = {0};       // 初始化的epoll事件
    ev.data.fd = fd;            // 更新描述符
    ev.events = events;         // 更新其中的事件
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev);    // epoll_ctl成功时返回0，否则返回-1
}

bool Epoller::ModFd(int fd, uint32_t events) {  // 修改文件描述符和感兴趣的事件类型(两者相互关联)
    if(fd < 0) return false;
    epoll_event ev = {0};
    ev.data.fd = fd;
    ev.events = events;
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &ev);    // 同样通过epoll_ctl处理
}

bool Epoller::DelFd(int fd) {   // 移除文件描述符，那么对应的事件应该为空，可以理解
    if(fd < 0) return false;
    epoll_event ev = {0};
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, &ev);
}

// events_[0]我理解为专用的用来处理阻塞的事件？
int Epoller::Wait(int timeoutMs) {
    return epoll_wait(epollFd_, &events_[0], static_cast<int>(events_.size()), timeoutMs);
}

int Epoller::GetEventFd(size_t i) const {   // 获取事件数组中索引i下的文件描述符
    assert(i < events_.size() && i >= 0);
    return events_[i].data.fd;
}

uint32_t Epoller::GetEvents(size_t i) const {   // 获取事件数组中索引i下的事件(数组的元素类型是一个结构体，包含了文件描述符和事件)
    assert(i < events_.size() && i >= 0);
    return events_[i].events;
}