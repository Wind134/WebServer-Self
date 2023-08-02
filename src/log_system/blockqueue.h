/**
 * 这是一个阻塞队列的头文件，阻塞队列的作用在于：先将缓冲区的数据保存在该队列中，先不写；
 * 写入操作交给专门的线程来负责；
 * 这样就很好的实现了异步操作；
 */
#ifndef BLOCKQUEUE_H
#define BLOCKQUEUE_H

#include <mutex>    // 包含互斥锁的头文件
#include <deque>    // 双端队列
#include <condition_variable>   // 用于线程同步的类，用于在多线程环境中进行条件变量的操作
#include <sys/time.h>


/**
 * @brief 下面定义的是一个类模板，代表了一个阻塞队列块，T代表的是队列中的元素数据类型，对这个块的封装实现了线程安全；
 */
template<class T>
class BlockDeque {
public:

    explicit BlockDeque(size_t MaxCapacity = 1000); // 必须显式构造

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

    bool pop(T &item, int timeout);

    void flush();

private:
    std::deque<T> deq_; // 双端队列deq_，队列中元素类型为T

    size_t capacity_;   // 阻塞队列容量

    std::mutex mtx_;    // 互斥锁，用于在多线程环境中对临界区资源进行互斥访问，以确保线程安全。

    bool isClose_;      // 记录阻塞队列的运行状态

    std::condition_variable condConsumer_;  // 用于线程同步，代表的是消费者变量

    std::condition_variable condProducer_;  // 用于线程同步，代表生产者变量
};


/**
 * @brief 构造函数，初始化了阻塞队列的大小，并设置队列的状态；
 * @param MaxCapacity 队列的最大容量；
 */
template<class T>
BlockDeque<T>::BlockDeque(size_t MaxCapacity) :capacity_(MaxCapacity) {
    assert(MaxCapacity > 0);
    isClose_ = false;   // 队列开启
}

/**
 * @brief 析构函数，执行关闭队列的功能；
 */
template<class T>
BlockDeque<T>::~BlockDeque() {
    Close();
};

/**
 * @brief 关闭整个阻塞队列，同样互斥访问；
*/
template<class T>
void BlockDeque<T>::Close() {
    {   
        std::lock_guard<std::mutex> locker(mtx_);
        
        // 下面两行代码的执行是在互斥条件下执行的，表明关闭过程不允许被中断
        deq_.clear();   // 清空队列信息
        isClose_ = true;// 将队列状态设定为关闭
    }

    // 完成之后通知所有等待在生产者条件变量和消费者条件变量上的线程，以唤醒它们；
    // 可以理解为别等了，各回各家了；
    condProducer_.notify_all();
    condConsumer_.notify_all();
};

/**
 * 手动刷新机制，手动唤醒一个消费者线程；
 */
template<class T>
void BlockDeque<T>::flush() {
    condConsumer_.notify_one();
};

/**
 * @brief 清除阻塞队列的所有内容，这部分的执行加上了互斥锁，以保证对临界区资源的独占；
 */
template<class T>
void BlockDeque<T>::clear() {
    std::lock_guard<std::mutex> locker(mtx_);
    deq_.clear();
}

/**
 * @brief 获取阻塞队列的前端元素，由于元素有出入操作，因此同样需要互斥访问；
 */
template<class T>
T BlockDeque<T>::front() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.front();
}

/**
 * @brief 获取阻塞队列的后端元素，由于元素有出入操作，因此同样需要互斥访问；
 */
template<class T>
T BlockDeque<T>::back() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.back();
}

/**
 * @brief 获取队列的实际大小，要确保获取的准确大小，加上了互斥锁；
 */
template<class T>
size_t BlockDeque<T>::size() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.size();
}

/**
 * @brief 获取队列的容量，如果阻塞队列并未涉及到容量大小的拓展，其实可以不需要互斥操作(个人理解)；
 */
template<class T>
size_t BlockDeque<T>::capacity() {
    std::lock_guard<std::mutex> locker(mtx_);
    return capacity_;
}

/**
 * @brief 给队列尾端插入一个相应类型的元素，插入元素对应着生产者的角色，这个过程同样不允许被打断；
 * @param item 要插入的元素；
 */
template<class T>
void BlockDeque<T>::push_back(const T &item) {
    std::unique_lock<std::mutex> locker(mtx_);
    while(deq_.size() >= capacity_) {   // 如果队列已满，那么生产者需要等待
        condProducer_.wait(locker);     // 等待的同时，把锁也给占用了，这个插入元素的线程被阻塞了，其他线程插入的操作也会被阻塞，锁还在他手上
    }
    // 能走出循环，说明有空间了，就可以执行操作了
    deq_.push_back(item);

    // 唤醒消费者线程，通知其有新的元素可以消费
    // 这一步的意义体现在，因为阻塞队列中的元素数量有限
    // 有的线程没有争抢到资源被阻塞，这个时候由于新的资源被生产了，因此可以唤醒这个线程
    condConsumer_.notify_one();
}

/**
 * @brief 给队列前端插入一个相应类型的元素，插入元素对应着生产者的角色，这个过程同样不允许被打断；
 * @param item 要插入的元素；
 */
template<class T>
void BlockDeque<T>::push_front(const T &item) {
    std::unique_lock<std::mutex> locker(mtx_);
    while(deq_.size() >= capacity_) {
        condProducer_.wait(locker);
    }
    deq_.push_front(item);
    condConsumer_.notify_one();
}

/**
 * @brief 判断队列是否为空，判空的这么一个步骤同样不允许打断，也加上了互斥锁保护；
 */
template<class T>
bool BlockDeque<T>::empty() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.empty();
}

/**
 * @brief 判断队列是否已满，这部分同样加锁，不能被打断；
*/
template<class T>
bool BlockDeque<T>::full(){
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.size() >= capacity_;
}

/**
 * @brief 给队列前端取出一个相应类型的元素，取出元素对应着消费者的角色，这个过程同样不允许被打断；
 * @param item 取出的元素给了item；
 * @return 是否成功取出；
 */
template<class T>
bool BlockDeque<T>::pop(T &item) {
    std::unique_lock<std::mutex> locker(mtx_);  // 锁
    while(deq_.empty()){    // 若空，则阻塞
        condConsumer_.wait(locker);
        if(isClose_){
            return false;
        }
    }
    item = deq_.front();
    deq_.pop_front();
    condProducer_.notify_one(); // 消费者线程负责唤醒被阻塞的生产者线程；
    return true;
}

/**
 * @brief 在设定的超时时间到了之后，从队列前端取出一个相应类型的元素，相当于增加了定时功能；
 * @param item 取出的元素给了item；
 * @param timeout 超时时间，秒为单位；
 * @return 是否成功取出；
 */
template<class T>
bool BlockDeque<T>::pop(T &item, int timeout) {
    std::unique_lock<std::mutex> locker(mtx_);
    while(deq_.empty())
    {
        // 在指定的超时时间内等待条件变量的信号(队列不空)，如果超时了(即未收到信号)，则执行相应的操作。
        // 为了防止无限等待，因此设定一个超时时间，超过了这个时间就不等了；
        // wait_for()函数在等待期间会返回一个std::cv_status枚举类型的值，表示等待的结果状态。
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