#include "epoller.h"

/**
 * @brief 构造函数创建epoll描述符，设定监测的最大事件参数；
 * @param epollFd_ epoll文件描述符；
 * @param events 监测的最大事件数；
 */
Epoller::Epoller(int maxEvent):epollFd_(epoll_create(512)), events_(maxEvent){
    assert(epollFd_ >= 0 && events_.size() > 0);    // 需要满足合理条件
}

/**
 * @brief 析构函数，关闭epoll描述符；
 */
Epoller::~Epoller() {
    close(epollFd_);    // 关闭epoll描述符
}

/**
 * @brief 添加文件描述符和感兴趣的事件类型(两者相互关联，信息糅在一个结构体当中)；
 * @param fd 要添加的文件描述符；
 * @param events 关注的事件类型；
 * @return 描述符的添加结果；
 */
bool Epoller::AddFd(int fd, uint32_t events) {  
    if(fd < 0) return false;    // 不合理的文件描述符
    epoll_event ev = {0};       // 初始化的epoll事件
    ev.data.fd = fd;            // 更新描述符
    ev.events = events;         // 更新其中的事件
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev);    // epoll_ctl成功时返回0，否则返回-1
}

/**
 * @brief 修改文件描述符和感兴趣的事件类型(两者相互关联)；
 * @param fd 要修改的文件描述符；
 * @param events 修改所关注的事件类型；
 * @return 描述符的添加结果；
 */
bool Epoller::ModFd(int fd, uint32_t events) {  // 
    if(fd < 0) return false;
    epoll_event ev = {0};
    ev.data.fd = fd;
    ev.events = events;
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &ev);    // 同样通过epoll_ctl处理
}

/**
 * @brief 从epoll中移除文件描述符，不监听该描述符了；
 * @param fd 要移除的文件描述符；
 * @return 移除的结果；
 */
bool Epoller::DelFd(int fd) {
    if(fd < 0) return false;
    epoll_event ev = {0};
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, &ev);
}

/**
 * @brief 获取相应时间内就绪的文件描述符数量；
 * @param timeoutMs 阻塞的时间，单位毫秒，参数为-1表明无限等待；
 * @return 阻塞时间到期后就绪的文件描述符数量；
 */
int Epoller::Wait(int timeoutMs) {
    // &events_[0]传入事件数组的首地址
    return epoll_wait(epollFd_, &events_[0], static_cast<int>(events_.size()), timeoutMs);
}

/**
 * @brief 获取事件数组指定位置的事件的文件描述符；(理解为某事件下对应的文件描述符)
 * @return 文件描述符；
 */
int Epoller::GetEventFd(size_t i) const {
    assert(i < events_.size() && i >= 0);
    return events_[i].data.fd;
}

/**
 * @brief 获取事件数组指定位置的事件；
 * @return 事件信息；
 */
uint32_t Epoller::GetEvents(size_t i) const {
    assert(i < events_.size() && i >= 0);
    return events_[i].events;
}