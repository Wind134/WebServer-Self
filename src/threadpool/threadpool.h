/*
- 这是一个基于源线程池修改的新的线程池，意在提升线程池的并发能力；
- 主要的核心优化点在于将新建立的线程提交进一个vector数组；
- 此外调整一些变量的设计；
*/ 
#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <mutex>    // 用于互斥
#include <condition_variable>   // 用于同步
#include <queue>    // 普通队列
#include <vector>   // 存线程
#include <thread>   // 调线程
#include <cassert>  // 断言关键字
#include <functional>   // 包含了一系列函数对象与函数模板
class ThreadPool {
public:
    /**
     * @brief 线程池的构造函数，定义为显式构造；
     * @param threadCount 线程池中的线程数量，默认为8(8线程)
     */
    explicit ThreadPool(size_t threadCount = 8): isClosed(false) {
            assert(threadCount > 0);
            // 下面这个for循环会创建8个不断工作的线程
            for(size_t i = 0; i < threadCount; i++) {
                multi_thread.emplace_back(
                    [this]() {  // this传入这个类的地址，从而lambda可以访问该类所有成员(包括私有，因为lambda对象属于类的一部分)
                        while (true) {
                            std::function<void()> task;
                            {
                                std::unique_lock<std::mutex> lock(mtx);
                                // 同步等待，直到线程池关闭或者任务队列非空了
                                cond_cv.wait(lock, [this] () {return isClosed || !tasks.empty();});
                                if (isClosed && tasks.empty())  return; // 可以结束lambda表达式了
                                task = std::move(tasks.front());
                                tasks.pop();
                            }
                            task(); // 任务的执行不需要锁，这一步可以尽可能的并发
                        }
                    }
                );
            }
    }

    // 下面将不需要的默认构造函数(拷贝，移动，拷贝赋值，移动赋值等)都设置为delete的，节省资源
    // 保留默认构造函数，因为上面定义的构造函数有一个默认值，这个默认的构造会用8个线程参数进行构造处理
    ThreadPool() = default;
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;

    ThreadPool& operator = (const ThreadPool&) = delete;
    ThreadPool& operator = (ThreadPool&&) = delete;

    
    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(mtx);
            isClosed = true;    // 设定线程池的关闭状态
        }
        cond_cv.notify_all();   // 唤醒所有被阻塞的消费者线程
        for (auto& threads : multi_thread)    // vector中的每个线程属于自己的任务需要完成
            threads.join(); // threads不能定义为const的，因为join之后threads的状态要被改变了
    }

    /**
     * @brief 这是一个提交任务进任务队列的函数，提交过程中会对线程池的状态进行检查；
     * @param f f是一个函数对象；
     * @param args args是函数的参数，由于是可变参，当然也可以为空；
     */
    template<typename F,typename... Args>
    void submit(F&& f,Args&&... args) {  // 传右值以保证完美转发
        {
            std::unique_lock<std::mutex> lock(mtx);
            if(isClosed) throw std::runtime_error("submit on stopped ThreadPool");
            tasks.emplace(std::bind(std::forward<F>(f),std::forward<Args>(args)...));   // 给f绑定了参数
        }
        cond_cv.notify_one();
    }

private:
    std::mutex mtx;     // 互斥锁
    std::condition_variable cond_cv;   // 面向消费者线程的用于同步和通信的条件变量
    bool isClosed;      // 表明池子开启与否的开关
    std::queue<std::function<void()>> tasks;    // 一个任务队列，任务队列的类型是函数对象，该函数不接受参数，返回void
    std::vector<std::thread> multi_thread;
};


#endif //THREADPOOL_H