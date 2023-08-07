/*
sqlconnpool头文件实现；

- 将连接理解为一个具体的对象，也就是说，对象是一个具体的连接；
- 数据库连接池的一个优异的机制就是，可以动态地管理和复用数据库连接，避免了频繁地创建和销毁连接的开销，提高了数据库连接的利用效率。
- 也就是说，某种程度上实现了复用，这个连接的创建和销毁都是针对与本地数据库的连接，而非登录用户信息的连接，那只是一段表查询而已；

细节：
- 在数据库连接池的初始化过程中，我们创建的所有连接，它的连接信息都是一致的(账户密码)
- 勘误：这个连接信息是连接数据库的信息，不是网页中用户登录的信息，之前会错意了
- 而这么做的目的就是为了实现连接的重用，但是如果想要对多个不同信息的连接做此处理
- 可能就需要重新考虑下整个连接池的设计了
*/ 
#include "sqlconnpool.h"
using namespace std;

/**
 * @brief 这是数据库连接池的构造函数，初始化已使用的连接和剩余的连接；
 */
SqlConnPool::SqlConnPool() {
    useCount_ = 0;  // 正在使用的连接，也就是从队列中pop出去的连接；
    freeCount_ = 0; // 初始化目前队列中的连接数；
}

/**
 * @brief 获取内存中那个唯一的数据库连接池实例；
 * @return 返回的是指向该数据库连接池的指针；
 */
SqlConnPool* SqlConnPool::Instance() {
    static SqlConnPool connPool;    // 构造实例，且保证仅有一个
    return &connPool;
}

/**
 * @brief 执行数据库连接池的初始化；
 * @param host 指向主机地址的指针；
 * @param port 端口号；
 * @param user 指向用户账户名的指针；
 * @param pwd 指向密码信息的指针；
 * @param dbName 指向数据库名字符的指针；
 * @param connSize 数据库的连接数；
 */
void SqlConnPool::Init(const char* host, int port,  // 主机地址，端口
            const char* user,const char* pwd, const char* dbName,   // 用户，密码，数据库名
            int connSize = 10) {    // 默认的连接数
    assert(connSize > 0);
    // 以默认参数为例，下面这个for循环建立了10个相同账户信息的连接
    // 比较疑惑的一点在于，这个连接池设计的初衷应该也有用于多用户连接的意图
    // 应该怎么处理这个意图呢？
    for (int i = 0; i < connSize; i++) {
        MYSQL *sql = nullptr;   // 初始化一个指向数据库连接的指针
        sql = mysql_init(sql);  // 数据库初始化
        if (!sql) {
            LOG_ERROR("MySql init error!"); // 初始化不成功则报错
            assert(sql);
        }
        sql = mysql_real_connect(sql, host, // 库函数，该函数进行连接，返回一个指向该连接的指针
                                 user, pwd,
                                 dbName, port, nullptr, 0);
        if (!sql) {
            LOG_ERROR("MySql Connect error!");  // 连接不成功则报错
        }
        connQue_.push(sql); // push一个连接
    }
    MAX_CONN_ = connSize;   // 更新最大连接数

    // 初始化一个命名或匿名的信号量对象，信号量的初始值为最大连接数MAX_CONN_
    sem_init(&semId_, 0, MAX_CONN_);
}

/**
 * @brief 获取一个数据库连接；
 * @return 返回一个指向数据库连接的指针；
*/
MYSQL* SqlConnPool::GetConn() {
    MYSQL *sql = nullptr;
    if(connQue_.empty()){   // 线程池已经将所有连接都取走了，它取走的数量等于MAX_CONN_，数据库很忙，一直在干活儿
        LOG_WARN("SqlConnPool busy!");  // 队列为空，表明现在数据库连接池已经将所有连接都用完了
        return nullptr;
    }

    // sem_wait()函数是一个POSIX信号量函数，用于尝试获取一个信号量资源。
    // 如果信号量的值大于0，则将信号量的值减1，并立即返回；
    // 如果信号量的值为0，则进程(或线程)将被阻塞，直到信号量的值大于0；
    sem_wait(&semId_);

    {   // 临界区，加锁
        lock_guard<mutex> locker(mtx_);
        sql = connQue_.front();
        connQue_.pop(); // 使用队列中的一个连接
        if (useCount_ < MAX_CONN_) ++useCount_;    // pop出去的应该就是要使用的
    }
    return sql; // 返回连接的地址
}

/**
 * @brief 将一个已经使用完的连接放入连接队列，后续如果需要使用这个连接可以取出，实现连接的重用而不必重复创建连接；
 */
void SqlConnPool::FreeConn(MYSQL* sql) {
    assert(sql);
    lock_guard<mutex> locker(mtx_);
    connQue_.push(sql); // 将连接压进去
    if (useCount_ > 0) --useCount_;
    // 通知其他线程，有一个新的可用连接了
    sem_post(&semId_);
}

/**
 * @brief 关闭连接池，关闭的过程实现了线程安全；
 */
void SqlConnPool::ClosePool() {
    lock_guard<mutex> locker(mtx_);
    while(!connQue_.empty()) {  // while循环保证关闭队列中所有的连接
        auto item = connQue_.front();
        connQue_.pop();
        mysql_close(item);  // 库函数，关闭一个sql连接
    }
    // 库函数，用于关闭MySQL库并释放相关资源
    mysql_library_end();        
}

/**
 * @brief 获取队列中数据库连接的数量，在队列中的都是可以重用的；
 */
int SqlConnPool::GetFreeConnCount() {
    lock_guard<mutex> locker(mtx_);
    return connQue_.size(); // 在队列中的，就代表可以使用的连接数量
}

/**
 * @brief 析构函数主要就是将数据库连接池关闭掉；
 */
SqlConnPool::~SqlConnPool() {
    ClosePool();
}
