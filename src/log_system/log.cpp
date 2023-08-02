#include "log.h"

using namespace std;

/**
 * @brief 私有的构造函数，初始化了各个成员；
 */
Log::Log() {
    lineCount_ = 0;
    isAsync_ = false;
    writeThread_ = nullptr;
    deque_ = nullptr;
    toDay_ = 0;
    fp_ = nullptr;
}

/**
 * @brief 析构函数要执行的两个功能：处理工作线程，处理尚未写完的日志文件；
 */
Log::~Log() {
    if(writeThread_ && writeThread_->joinable()) {  // 如果线程存在且处于可加入状态，我们需要执行下面的循环
        while(!deque_->empty()) {   // 如果队列不空，那我们要确保所有未写入文件的日志信息刷新到目标文件
            deque_->flush();        // 更具体点来说，如果deque_不空，那我们必须要唤醒一个消费者，要确保队列中的信息都进入文件
        };
        deque_->Close();
        writeThread_->join();   // 等待写入线程执行完毕，该线程将所有信息都写入到日志文件
    }
    // 处理完队列之后还需要处理文件描述符
    if(fp_) {
        lock_guard<mutex> locker(mtx_);
        flush();
        fclose(fp_);
    }
}

// 获取日志等级，这个过程需要加锁，因为全局只有一个日志实例，这意味着Log本身是共享的；
// 因此诸多操作都需要考虑线程安全；
int Log::GetLevel() {
    lock_guard<mutex> locker(mtx_);
    return level_;
}

/**
 * @brief 设置日志级别；
 */
void Log::SetLevel(int level) {
    lock_guard<mutex> locker(mtx_);
    level_ = level;
}

/**
 * @brief 初始化日志系统，包括开启日志系统，设定日志等级，设定日志的写入模式(同步或者异步)；
 * @param level 日志级别，默认级别是1；
 * @param path 日志保存的路径；
 * @param suffix 日志文件的后缀；
 * @param maxQueueSize 最大队列长度，给阻塞队列用的；
 */
void Log::init(int level = 1, const char* path, const char* suffix,
    int maxQueueSize) {
    isOpen_ = true;
    level_ = level;
    if(maxQueueSize > 0) 
    {
        isAsync_ = true;    // 开启异步模式
        if(!deque_) // 如果队列非空，new出两个对象，并用Log内置的智能指针指向(为啥不直接指向...)
        {
            // 回答上面的问题，因为deque_ = new BlockDeque<std::string>; 是非法的；
            // 因此只能通过下面的语句先创建一个临时智能指针对象，再通过移动语义交给deque_；
            // 或者使用C++ 14的make_unique；  
            // unique_ptr<BlockDeque<std::string>> newDeque(new BlockDeque<std::string>);
            // deque_ = move(newDeque);    // 使用C++ 11的移动语义转移所有权
            
            // unique_ptr<std::thread> NewThread(new thread(FlushLogThread)); // 用FlushLogThread初始化thread对象
            // writeThread_ = move(NewThread);
            deque_ = make_unique<BlockDeque<string>>(); 
            writeThread_ = make_unique<thread>(FlushLogThread); // 指向的是FlushLogThread线程
        }
    } 
    else    isAsync_ = false;   // 如果没有设定阻塞队列的长度，那么表明不需要开启异步日志机制，而是采用同步机制；

    lineCount_ = 0; // 记录已经写入日志的行数，初始化后设置为0

    time_t timer = time(nullptr);   // 获取当前的时间戳，并赋值给timer
    
    // localtime函数将时间戳转换为本地时间，并将结果保存在sysTime变量中。
    // localtime函数将时间戳转换为一个tm结构体，该结构体包含了年、月、日、时、分、秒等时间信息。
    // localtime结构体的时间从1900年开始算的。
    struct tm *sysTime = localtime(&timer); 
    struct tm t = *sysTime; // 时间信息都赋值给了t
    
    // 根据给定信息设置路径和日志信息
    path_ = path;
    suffix_ = suffix;
    char fileName[LOG_NAME_LEN] = {0};  // 日志文件名字符串，限制了最长文件名

    // 使用snprintf函数根据指定的格式将路径、日期和后缀组合成完整的日志文件名，并将结果存储在fileName中
    // 长度限制为LOG_NAME_LEN - 1，因为要留一个位置给空字符串'\0'
    snprintf(fileName, LOG_NAME_LEN - 1, "%s/%04d_%02d_%02d%s", 
            path_, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, suffix_);
    
    toDay_ = t.tm_mday; // 当前日期

    // 以下这部分加锁执行，不可打断，从这里我们可以看到
    // 加锁的范围越小越好，越小并发性越强。
    // 以下是针对文件的处理，这部分不允许被打断；
    {
        lock_guard<mutex> locker(mtx_);
        buff_.RetrieveAll();    // 清空缓冲区
        
        // 如果存在某个文件描述符，按理来说，刚新建的类中不应该存在文件的，但如果有，我们也需要对其处理；
        // 再重新解释下这段东西，这并不是新建的类，而是一个初始化操作，也就是说，即便是类已经存在了很久；
        // 我们仍然可以继续做初始化操作，可以立即为对日志系统做一个新的初始化；
        // 可能是想改路径，也可能是想改文件后缀名等
        // 但如果先前的文件处理过程没有完成，我们要先处理，再去打开一个新的文件；
        if(fp_)
        {
            flush();    // 这个函数负责处理将(阻塞队列中)相关的日志信息写入到文件中
            fclose(fp_); 
        }

        fp_ = fopen(fileName, "a"); // 新的文件描述符
        if(fp_ == nullptr) {
            mkdir(path_, 0777);     // 赋予权限
            fp_ = fopen(fileName, "a");
        } 
        assert(fp_ != nullptr);
    }
}

/**
 * @brief 日志的写入操作，注意，这个写入是向缓冲区写入信息；
 * @param level 日志级别；
 * @param format 格式化字符串，比如说printf中的("the %dth file:", n)，一种特殊的字符串形式；
 */
void Log::write(int level, const char *format, ...) {
    struct timeval now = {0, 0};    // 起始秒数，微秒数，每次写都会获取准确的时间信息，以决定是否要加后缀
    gettimeofday(&now, nullptr);    // 第一个参数是now的地址，第二个参数存储时区信息，我们暂且不管时区
    time_t tSec = now.tv_sec;       // 获取秒数
    struct tm *sysTime = localtime(&tSec);  // 基于1900年1月1日获取具体时间
    struct tm t = *sysTime;
    va_list vaList;                 // va_list是一个可变参数类型

    // 日志日期 日志行数
    // 第一部分的条件是不同日期的情况
    // 第二部分的条件是日志已经写满的情况，两个条件只要有一个满足，说明要建立新的日志文件了
    // 所以这部分判断语句的功能是：如果日期发生变化，或者日志写满，则建立新的日志记录
    if (toDay_ != t.tm_mday || (lineCount_ && (lineCount_  %  MAX_LINES == 0)))
    {
        // unique_lock<mutex> locker(mtx_);
        // locker.unlock();
        
        // 下面这一段不需要锁
        char newFile[LOG_NAME_LEN];
        char tail[36] = {0};

        // 说明一下该函数的功能，tail是一个字符串，最长为36，
        snprintf(tail, 36, "%04d_%02d_%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);

        if (toDay_ != t.tm_mday)    // 若是新一天的日志，则起一个新的文件
        {
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s%s", path_, tail, suffix_);
            toDay_ = t.tm_mday;
            lineCount_ = 0;
        }
        else {  // 如果是同一天，则直接在文件名后面加上序号
            // (lineCount_  / MAX_LINES)表示这一天第**份日志文件
            // MAX_LINES体现了每份日志文件能写入的最大行数
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s-%d%s", path_, tail, (lineCount_  / MAX_LINES), suffix_);
        }
        
        // // 重新加锁，这么更改是为了调整锁的粒度
        // locker.lock();
        unique_lock<mutex> locker(mtx_);
        flush();    // 刷新之前的数据，处理完之后，准备向新的日志文件写入信息
        fclose(fp_);
        fp_ = fopen(newFile, "a");  // 创建了文件描述符
        assert(fp_ != nullptr);
    }

    {   // 以下又是临界区资源
        unique_lock<mutex> locker(mtx_);
        lineCount_++;

        // n表示的是成功向缓冲区写入的字符总数
        // 在C的struct tm结构中，月份的取值范围是0到11
        int n = snprintf(buff_.BeginWrite(), 128, "%d-%02d-%02d %02d:%02d:%02d.%06ld ",
                    t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                    t.tm_hour, t.tm_min, t.tm_sec, now.tv_usec);
        
        // 更新写入到的位置            
        buff_.HasWritten(n);
        // 向缓冲区写入日志等级信息，这些全是向缓冲区写入
        AppendLogLevelTitle_(level);

        va_start(vaList, format);   // format将是一个字符串格式，后面的可变参数列表都传给vaList，format代表参数起始点(不包含format)

        // 接受可变参数版本的vsnprintf函数，m表示成功写入缓冲区的字节数，这里能写满就写满了
        // format同样是格式化的字符串
        int m = vsnprintf(buff_.BeginWrite(), buff_.WritableBytes(), format, vaList);
        va_end(vaList);

        // 更新写入指针的位置
        buff_.HasWritten(m);

        // 换行，追加相应字符，缓冲区实现了自动增长的功能
        // 因此不必考虑加不满的情况
        buff_.Append("\n\0", 2);
        
        // 如果开启了日志的异步写入，且双端队列存在且不满，那么可以直接将字符串放进阻塞队列
        // 如果放不进去那只能直接写入了
        if(isAsync_ && deque_ && !deque_->full()) {
            deque_->push_back(buff_.RetrieveAllToStr());    // 也相当于执行了生产者操作了
        } else {
            fputs(buff_.Peek(), fp_);   // 否则就直接写入文件
        }

        // 清空缓冲区
        buff_.RetrieveAll();

        // 以上整个过程都加锁
    }
}

/**
 * @brief  向缓冲区写入日志级别信息，一共四种级别；
 */
void Log::AppendLogLevelTitle_(int level) {
    switch(level) {
    case 0:
        buff_.Append("[debug]: ", 9);
        break;
    case 1:
        buff_.Append("[info] : ", 9);
        break;
    case 2:
        buff_.Append("[warn] : ", 9);
        break;
    case 3:
        buff_.Append("[error]: ", 9);
        break;
    default:
        buff_.Append("[info] : ", 9);
        break;
    }
}

/**
 * @brief 如果开启了异步执行机制，首先要做的是唤醒一个阻塞的消费者线程，而后，调用库中的fflush，将缓冲区的数据写入到文件中，
 */
void Log::flush() 
{
    // 如果开启了异步模式
    if(isAsync_)    deque_->flush();    // 这个时候缓冲区往往涉及到新字符信息的写入，这个时候就可以唤醒一个阻塞的消费者线程
    
    // 手动调用fflush确保缓冲区的数据立即写入文件，(但我没看到哪里执行的写入步骤啊)
    fflush(fp_);
}

/**
 * @brief 异步写入的执行过程，将阻塞队列中的信息按照从前到后的顺序写入到日志文件(这也正是消费者线程)；
 */
void Log::AsyncWrite_() {
    string str = "";
    // pop出来的队前元素的值赋给了str
    while(deque_->pop(str)) {
        lock_guard<mutex> locker(mtx_);
        fputs(str.c_str(), fp_);
    }
}

/**
 * @brief 单例模式下，在类的内部去构造一个实例，且是唯一的实例；
 * @return 返回一个指向该实例的指针；
 */
Log* Log::Instance() {
    static Log inst;    // 全局的日志实例，保证了在整个应用程序中只有这一个实例，由于是static的，作用域结束也不会被销毁；
    return &inst;
}

/**
 * @brief 调用异步写入的线程；
 */
void Log::FlushLogThread() {
    Log::Instance()->AsyncWrite_();
}