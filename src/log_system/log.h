/**
 * 这是异步日志系统实现的关键之处；
 * 实现了在运行的同时，有专门用于处理日志的线程；
 */
#ifndef LOG_H
#define LOG_H

#include <mutex>
#include <string>
#include <thread>   // 使用线程的头文件
#include <sys/time.h>
#include <string.h>
#include <stdarg.h>           // stdarg.h中定义了一组宏和函数，用于在函数定义和调用中处理可变数量的参数。
#include <assert.h>
#include <sys/stat.h>         // 声明了一些与文件状态和文件系统相关的函数和宏。
#include "blockqueue.h"
#include "../data_buffer/buffer.h"

class Log {
public:
    // 这里执行默认构造以及析构

    void init(int level, const char* path = "./log", 
                const char* suffix =".log",
                int maxQueueCapacity = 1024);

    static Log* Instance();
    static void FlushLogThread();

    void write(int level, const char *format,...);
    void flush();

    int GetLevel();
    void SetLevel(int level);
    bool IsOpen() { return isOpen_; }
    
private:
    Log();  // 构造函数设计为private的，其用意在于实现单例模式
    void AppendLogLevelTitle_(int level);
    virtual ~Log();
    void AsyncWrite_();

private:
    static const int LOG_PATH_LEN = 256;    // 路径字符串长度
    static const int LOG_NAME_LEN = 256;    // 日志文件名字长
    static const int MAX_LINES = 50000;

    const char* path_;      // 日志文件路径
    const char* suffix_;    // 日志文件后缀名

    int MAX_LINES_;

    int lineCount_; // 记录已经写入日志的行数
    int toDay_;     // 当前日期，构造函数会将之设置为0

    bool isOpen_;   // 记录日志系统的开启状态
    
    Buffer buff_;   // 缓冲池
    int level_;     // 日志等级
    bool isAsync_;  // 控制同步或是异步写入日志

    FILE* fp_;      // 指向文件描述符的指针
    std::unique_ptr<BlockDeque<std::string>> deque_;    // 指向一个元素类型为string的异步队列
    std::unique_ptr<std::thread> writeThread_;          // 通过unique_ptr保证(写日志线程)对象的资源正确释放
    std::mutex mtx_;    // 互斥锁
};

// 定义了一个LOG_BASE的宏，执行类似函数的功能，参数为可变参数，其中前两个：日志打印级别，格式化字符串
// '\'表示宏可以跨越多行，这个标志必须存在
// log为指向一个log实例的指针
// '##'操作符用于连接前面的标识符(如果存在)和可变参数
#define LOG_BASE(level, format, ...) \
    do {\
        Log* log = Log::Instance();\
        if (log->IsOpen() && log->GetLevel() <= level) {\
            log->write(level, format, ##__VA_ARGS__); \
            log->flush();\
        }\
    } while(0);

// 下面这个宏定义了四种日志级别，DEBUG INFO WARN ERROR分别对应0 1 2 3四种日志级别
#define LOG_DEBUG(format, ...) do {LOG_BASE(0, format, ##__VA_ARGS__)} while(0);
#define LOG_INFO(format, ...) do {LOG_BASE(1, format, ##__VA_ARGS__)} while(0);
#define LOG_WARN(format, ...) do {LOG_BASE(2, format, ##__VA_ARGS__)} while(0);
#define LOG_ERROR(format, ...) do {LOG_BASE(3, format, ##__VA_ARGS__)} while(0);

#endif //LOG_H
