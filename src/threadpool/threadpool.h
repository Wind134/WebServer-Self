/*
- 这是一个线程池的定义，接下来针对该线程池进行逐步分析
*/ 

#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <mutex>    // 用于互斥
#include <condition_variable>   // 用于同步
#include <queue>    // 普通队列
#include <thread>   // 调线程
#include <cassert>  // 断言关键字
#include <functional>   // 包含了一系列函数对象与函数模板
class ThreadPool {
public:
    // explicit关键字用于避免意外的类型转换
    explicit ThreadPool(size_t threadCount = 8): pool_(std::make_shared<Pool>()) {
            assert(threadCount > 0);
            // 下面这个for循环会创建8个不断工作的线程
            for(size_t i = 0; i < threadCount; i++) {
                // 用一个lambda函数对象初始化一个线程对象，这里需要对lambda有详细的认识
                // 捕获列表中，将pool_指针传给了pool
                std::thread([pool = pool_] {
                    // 用池中的mtx锁住，这个过程不允许打断
                    std::unique_lock<std::mutex> locker(pool->mtx);
                    while(true) {
                        // 在函数对象队列非空的情况下
                        if(!pool->tasks.empty()) {
                            // 用了C++ 11的新特性，这样就不需要拷贝了，提升性能
                            auto task = std::move(pool->tasks.front());
                            // 弹出最前的元素
                            pool->tasks.pop();
                            locker.unlock();
                            // task的执行无所谓，不需要保证互斥性
                            task();
                            locker.lock();
                        }
                        // 线程池为空的情况下，如果池关了，那么说明任务做完了，可以退出这个循环 
                        else if(pool->isClosed) break;
                        // 如果线程池为空，且池还是开的，那么等待一个条件变量
                        // 线程会释放锁，等待唤醒
                        else pool->cond.wait(locker);
                    }
                }).detach();    // detach可以将线程分离，可以与主线程并行执行，主线程不需要等待
            }
    }

    // 默认构造函数
    ThreadPool() = default;

    // C++11标准的移动构造函数
    ThreadPool(ThreadPool&&) = default;
    
    ~ThreadPool() {
        if(static_cast<bool>(pool_)) {  // 检测pool_的存在性，nullptr->false？对吗？，为啥不直接判断嘞
            {
                std::lock_guard<std::mutex> locker(pool_->mtx);
                pool_->isClosed = true;
            }

            // 通知所有正在等待的线程，这些正在等待的线程有一个特征就是，因为此刻队列为空，但是
            // isClosed变量没有检测到true，因此这些线程都被阻塞，因此线程的特征：因为队列为空，且队列未关闭，而被阻塞。
            // 通过下行的代码，会唤醒这些进程，然后这些进程知道了isClosed的状态，全部都会break退出循环，线程关闭
            pool_->cond.notify_all();
        }
    }

    // 添加任务的函数，观察参数，发现需要是右值
    // 这样的设计主要是为了避免拷贝，从而提高效率
    template<class F>
    void AddTask(F&& task) {
        {
            std::lock_guard<std::mutex> locker(pool_->mtx);
            pool_->tasks.emplace(std::forward<F>(task));
        }
        pool_->cond.notify_one();
    }

private:
    // 这是一个池子
    struct Pool {
        std::mutex mtx;     // 池子里有互斥锁
        std::condition_variable cond;   // 有用于同步和通信的条件变量
        bool isClosed;      // 表明池子开启与否的开关
        std::queue<std::function<void()>> tasks;    // 一个任务队列，任务队列的类型是函数对象，该函数不接受参数，返回void
    };
    // 指向池子的智能指针，因为会被多个线程使用，因此定义为shared_ptr，这里可以联系之前unique_ptr指针指向的日志队列理解
    std::shared_ptr<Pool> pool_;
};


#endif //THREADPOOL_H