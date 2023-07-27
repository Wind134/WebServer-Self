#ifndef BLOCKQUEUE_H
#define BLOCKQUEUE_H

#include <mutex>    // 包含互斥锁的头文件
#include <deque>    // 双端队列
#include <condition_variable>   // 用于线程同步的类，用于在多线程环境中进行条件变量的操作
#include <sys/time.h>


// 下面定义的是一个类模板，代表了一个队列块，T代表的是队列中的元素数据类型
// 对这个块的封装实现了线程安全
template<class T>
class BlockDeque {
public:

    // explicit关键字要求显式构造，默认队列的最大容量为1000
    explicit BlockDeque(size_t MaxCapacity = 1000);

    ~BlockDeque();  // 析构函数

    // 清除队列元素
    void clear();

    // 判空
    bool empty();

    // 判满
    bool full();

    // 关闭队列块
    void Close();

    // 获取队列大小
    size_t size();

    // 获取队列容量
    size_t capacity();

    // 队列中第一个元素
    T front();

    // 队列中最后一个元素
    T back();

    // 在后端插入一个元素，扮演生产者
    void push_back(const T &item);

    // 在前端插入一个元素，扮演生产者
    void push_front(const T &item);

    // 判断pop操作是否成功，同时pop出来的元素赋给了item
    // 扮演消费者的功能
    bool pop(T &item);

    // 相比上一行，增加了一个超时判断机制
    bool pop(T &item, int timeout);

    // 唤醒一个消费者线程
    void flush();

private:
    std::deque<T> deq_; // 双端队列deq_，队列中元素类型为T

    size_t capacity_;   // 所以是队列本身的容量对吗？

    std::mutex mtx_;    // 互斥锁，用于在多线程环境中对共享资源进行互斥访问，以确保线程安全。

    bool isClose_;

    std::condition_variable condConsumer_;  // 用于线程同步，代表的是消费者变量

    std::condition_variable condProducer_;  // 用于线程同步，代表生产者变量
};


// 构造这个队列块时，队列本身的状态是打开的
template<class T>
BlockDeque<T>::BlockDeque(size_t MaxCapacity) :capacity_(MaxCapacity) {
    assert(MaxCapacity > 0);
    isClose_ = false;
}

template<class T>
BlockDeque<T>::~BlockDeque() {
    Close();
};

// Close过程是清空队列元素的过程，并将状态设定为关闭
template<class T>
void BlockDeque<T>::Close() {
    {   
        std::lock_guard<std::mutex> locker(mtx_);
        
        // 下面两行代码的执行是在互斥条件下执行的，表明关闭过程不允许被中断
        deq_.clear();
        isClose_ = true;
    }

    // 完成之后通知所有等待在生产者条件变量和消费者条件变量上的线程，以唤醒它们。
    condProducer_.notify_all();
    condConsumer_.notify_all();
};

// 告诉消费者线程，可以出来干活了，需要去消费队列块中的某个元素了
// 也可以理解为刷新一种状态
template<class T>
void BlockDeque<T>::flush() {
    condConsumer_.notify_one();
};

// 函数的执行保证了线程安全，表明队列的清空操作不允许被打断
template<class T>
void BlockDeque<T>::clear() {
    std::lock_guard<std::mutex> locker(mtx_);
    deq_.clear();
}

template<class T>
T BlockDeque<T>::front() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.front();
}

template<class T>
T BlockDeque<T>::back() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.back();
}

// 获取队列实际大小
template<class T>
size_t BlockDeque<T>::size() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.size();
}

// 获取容量
template<class T>
size_t BlockDeque<T>::capacity() {
    std::lock_guard<std::mutex> locker(mtx_);
    return capacity_;
}

// 生产者进程
template<class T>
void BlockDeque<T>::push_back(const T &item) {
    std::unique_lock<std::mutex> locker(mtx_);
    while(deq_.size() >= capacity_) {   // 如果队列已满，那么生产者需要等待
        condProducer_.wait(locker);
    }
    // 当有空间了，就可以执行操作了
    deq_.push_back(item);

    // 唤醒消费者线程，通知其有新的元素可以消费
    condConsumer_.notify_one();
}

template<class T>
void BlockDeque<T>::push_front(const T &item) {
    std::unique_lock<std::mutex> locker(mtx_);
    while(deq_.size() >= capacity_) {
        condProducer_.wait(locker);
    }
    deq_.push_front(item);
    condConsumer_.notify_one();
}

// 判空操作同样保证了线程安全
template<class T>
bool BlockDeque<T>::empty() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.empty();
}

// 队列的实际大小大于给定的容量则表明队列已满
template<class T>
bool BlockDeque<T>::full(){
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.size() >= capacity_;
}

// 这是消费者进程，双端队列前面的元素都赋给了item(因为是引用)
template<class T>
bool BlockDeque<T>::pop(T &item) {
    std::unique_lock<std::mutex> locker(mtx_);
    while(deq_.empty()){
        condConsumer_.wait(locker);
        if(isClose_){
            return false;
        }
    }
    item = deq_.front();
    deq_.pop_front();
    condProducer_.notify_one();
    return true;
}

template<class T>
bool BlockDeque<T>::pop(T &item, int timeout) {
    std::unique_lock<std::mutex> locker(mtx_);
    while(deq_.empty())
    {
        if(condConsumer_.wait_for(locker, std::chrono::seconds(timeout)) 
                == std::cv_status::timeout){
            return false;
        }
        if(isClose_){
            return false;
        }
    }
    item = deq_.front();
    deq_.pop_front();
    condProducer_.notify_one();
    return true;
}

#endif // BLOCKQUEUE_H