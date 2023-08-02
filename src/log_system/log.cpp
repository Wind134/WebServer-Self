#include "log.h"

using namespace std;

// 默认构造函数
Log::Log() {
    lineCount_ = 0;
    isAsync_ = false;
    writeThread_ = nullptr;
    deque_ = nullptr;
    toDay_ = 0;
    fp_ = nullptr;
}

// 析构函数
Log::~Log() {
    if(writeThread_ && writeThread_->joinable()) {  // 如果线程存在且处于可加入状态，我们需要执行下面的循环
        while(!deque_->empty()) {   // 如果队列不空，那我们要确保所有未写入的日志刷新到目标文件
            deque_->flush();    // 更具体点来说，如果deque_不空，那我们必须要唤醒一个消费者，要确保队列中的信息都用到
        };
        deque_->Close();
        writeThread_->join();   // 等待写入线程执行完毕
    }
    // 处理完队列之后还需要处理文件描述符
    if(fp_) {
        lock_guard<mutex> locker(mtx_);
        flush();
        fclose(fp_);
    }
}

// 获取日志等级，这个过程需要加锁
int Log::GetLevel() {
    lock_guard<mutex> locker(mtx_);
    return level_;
}

void Log::SetLevel(int level) {
    lock_guard<mutex> locker(mtx_);
    level_ = level;
}

// 初始化的工作内容，开启日志系统，设定日志等级，设定日志的写入模式(同步或者异步)
void Log::init(int level = 1, const char* path, const char* suffix,
    int maxQueueSize) {
    isOpen_ = true;
    level_ = level;
    if(maxQueueSize > 0) 
    {
        isAsync_ = true;    // 开启异步模式
        if(!deque_) // 如果队列非空，new出两个对象，并用Log内置的智能指针指向(为啥不直接指向...)
        {       
            unique_ptr<BlockDeque<std::string>> newDeque(new BlockDeque<std::string>);
            deque_ = move(newDeque);    // 这就是unique_ptr的移动语义？
            
            unique_ptr<std::thread> NewThread(new thread(FlushLogThread)); // 用FlushLogThread初始化thread对象
            writeThread_ = move(NewThread);
        }
    } 
    else    isAsync_ = false;   // 如果队列块为空，那么关闭异步模式

    lineCount_ = 0; // 记录已经写入日志的行数，初始化后设置为0

    time_t timer = time(nullptr);   // 获取当前的时间戳，并赋值给timer
    
    // localtime函数将时间戳转换为本地时间，并将结果保存在sysTime变量中。
    // localtime函数将时间戳转换为一个tm结构体，该结构体包含了年、月、日、时、分、秒等时间信息。
    struct tm *sysTime = localtime(&timer); 
    struct tm t = *sysTime; // 时间信息都赋值给了t
    
    // 根据给定信息设置路径和日志信息
    path_ = path;
    suffix_ = suffix;
    char fileName[LOG_NAME_LEN] = {0};  // 初始为空

    // 使用snprintf函数根据指定的格式将路径、日期和后缀组合成完整的日志文件名，并将结果存储在fileName中
    snprintf(fileName, LOG_NAME_LEN - 1, "%s/%04d_%02d_%02d%s", 
            path_, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, suffix_);
    
    toDay_ = t.tm_mday; // 当前日期

    // 以下这部分加锁执行，不可打断
    {
        lock_guard<mutex> locker(mtx_);
        buff_.RetrieveAll();    // 清空缓冲区
        
        if(fp_) // 如果存在某个文件描述符 
        {
            flush();
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

void Log::write(int level, const char *format, ...) {
    struct timeval now = {0, 0};    // 起始秒数，微秒数
    gettimeofday(&now, nullptr);    // 第一个参数是now的地址，第二个参数存储时区信息，我们暂且不管时区
    time_t tSec = now.tv_sec;       // 获取秒数
    struct tm *sysTime = localtime(&tSec);  // 基于1970年1月1日获取具体时间
    struct tm t = *sysTime;
    va_list vaList;                 // va_list是一个可变参数类型

    /* 日志日期 日志行数 */
    // 第一部分的条件是如果是不同日期的情况，第二部分的条件是，如果日志已经写入了且写满了，两个条件只要有一个满足，说明要写入新的日志了
    // 意思就是当日志是之前的时间写入的，或者日志写入了，且写满了
    if (toDay_ != t.tm_mday || (lineCount_ && (lineCount_  %  MAX_LINES == 0)))
    {
        unique_lock<mutex> locker(mtx_);
        locker.unlock();
        
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
        else {
            // (lineCount_  / MAX_LINES)表示第**份日志文件，这个好理解
            // MAX_LINES体现了每份日志文件能写入的最大行数
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s-%d%s", path_, tail, (lineCount_  / MAX_LINES), suffix_);
        }
        
        // 重新加锁
        locker.lock();
        flush();
        fclose(fp_);
        fp_ = fopen(newFile, "a");  // 创建了文件描述符
        assert(fp_ != nullptr);
    }

    {
        unique_lock<mutex> locker(mtx_);
        lineCount_++;

        // n表示的是成功写入的字符总数
        // 在C的struct tm结构中，月份的取值范围是0到11
        int n = snprintf(buff_.BeginWrite(), 128, "%d-%02d-%02d %02d:%02d:%02d.%06ld ",
                    t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                    t.tm_hour, t.tm_min, t.tm_sec, now.tv_usec);
        
        // 更新写入到的位置            
        buff_.HasWritten(n);
        // 写入日志标题
        AppendLogLevelTitle_(level);

        va_start(vaList, format);   // format将是一个字符串格式，后面的可变参数列表都传给vaList

        // 接受可变参数版本的vsnprintf函数，m表示成功写入的字节数
        int m = vsnprintf(buff_.BeginWrite(), buff_.WritableBytes(), format, vaList);
        va_end(vaList);

        // 更新写入指针的位置
        buff_.HasWritten(m);

        // 换行，追加相应字符
        buff_.Append("\n\0", 2);
        
        // 如果开启了日志的异步写入，且双端队列存在且不满，那么可以直接将字符串放进双端队列
        if(isAsync_ && deque_ && !deque_->full()) {
            deque_->push_back(buff_.RetrieveAllToStr());
        } else {
            fputs(buff_.Peek(), fp_);   // 否则就直接写入文件
        }

        // 清空缓冲区
        buff_.RetrieveAll();

        // 以上整个过程都加锁
    }
}

// 添加日志级别标题
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

void Log::flush() 
{
    // 如果开启了异步模式
    if(isAsync_)    deque_->flush();    // 通知线程干活，对已存在的元素进行消费
    
    // 手动调用fflush确保数据立即写入文件，(但我没看到哪里执行的写入步骤啊)
    fflush(fp_);
}

// 异步写入的操作过程
void Log::AsyncWrite_() {
    string str = "";
    // pop出来的队前元素的值赋给了str
    while(deque_->pop(str)) {
        lock_guard<mutex> locker(mtx_);
        fputs(str.c_str(), fp_);
    }
}

Log* Log::Instance() {
    static Log inst;    // 全局的日志实例，保证了在整个应用程序中只有这一个实例
    return &inst;
}

// 日志写入操作放在单独的线程中，可以避免阻塞主线程的执行，提高日志写入的性能
void Log::FlushLogThread() {
    Log::Instance()->AsyncWrite_();
}