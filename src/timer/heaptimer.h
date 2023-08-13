/*
介绍：
- 这是一个基于堆的定时器，可以处理一些超时连接的任务；
- 主要的机制就是调用回调函数，执行相应的工作；
*/ 
#ifndef HEAP_TIMER_H
#define HEAP_TIMER_H

#include <queue>
#include <unordered_map>
#include <time.h>
#include <algorithm>
#include <arpa/inet.h> 
#include <functional> 
#include <assert.h> 
#include <chrono>
#include "../log_system/log.h"

/**
 * @brief 定义了一个函数对象类型，这是一个回调函数，超时则执行该函数；
 */
typedef std::function<void()> TimeoutCallBack;

/**
 * @brief 定义了一个高精度时钟类型；
 */
typedef std::chrono::high_resolution_clock Clock;

/**
 * @brief 定义了一个毫秒级别的时间单位；
 */
typedef std::chrono::milliseconds MS;

/**
 * @brief 一个简单的别名定义；
 */
typedef Clock::time_point TimeStamp;    // 给时间点定义一个别名

/**
 * @brief TimerNode结构体可以用来表示一个定时器节点，该结构体可以用于实现定时任务、事件调度等功能。
 * @param id 节点的唯一标识符；
 * @param expires 过期时间点；
 * @param cd 回调函数；
 * @param < 被重载的小于运算符；
 */
struct TimerNode {
    int id;
    TimeStamp expires;  // 理解为消亡的时间点？
    TimeoutCallBack cb; // cb是一个函数
    bool operator<(const TimerNode& t) {    // 重载了比较运算符'<'
        return expires < t.expires;
    }
};

/**
 * @brief 基于小根堆的定时器类，类中附带了一系列成员以及方法；
 */
class HeapTimer {
public:
    /**
     * @brief 构造函数，只干了一件事，预留了堆的空间；
     */
    HeapTimer() { heap_.reserve(64); }  // 预留64个TimerNode

    /**
     * @brief 析构函数，任务是清空所有内容；
     */
    ~HeapTimer() { clear(); }   // 析构，清空内容
    
    void adjust(int id, int timeout);

    void add(int id, int timeOut, const TimeoutCallBack& cb);

    void doWork(int id);

    void clear();   // 清空所有内容

    void tick();    // 清除超时结点

    void pop();     // 推出最前面的元素

    int GetNextTick();  // 获取当前最近的定时器到期时间，以毫秒为单位返回时间间隔。

private:
    void del_(size_t i);    // 删除指定位置的结点
    
    void siftup_(size_t i); // 使得定时器满足最小堆的性质

    bool siftdown_(size_t index, size_t n); // 将索引index处的位置下沉，以满足最小堆的性质

    // 在定时器节点数组中交换索引i, j的定时器节点位置
    void SwapNode_(size_t i, size_t j);

    std::vector<TimerNode> heap_;   // 定义了一个TimerNode的数组(起始本质上是维护了一个堆(堆是完全二叉树))

    std::unordered_map<int, size_t> ref_;   // 定时器节点ID->heap_位置的映射；
};

#endif //HEAP_TIMER_H
