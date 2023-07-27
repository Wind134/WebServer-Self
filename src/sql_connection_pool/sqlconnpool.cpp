/*
sqlconnpool头文件实现；

- 将连接理解为一个具体的对象，也就是说，对象是一个具体的连接；

- 数据库连接池的一个优异的机制就是，可以动态地管理和复用数据库连接，避免了频繁地创建和销毁连接的开销，提高了数据库连接的利用效率。
- 也就是说，某种程度上实现了复用
*/ 
#include "sqlconnpool.h"
using namespace std;

SqlConnPool::SqlConnPool() {
    useCount_ = 0;  // 这两个变量目前好像没有用上
    freeCount_ = 0;
}

SqlConnPool* SqlConnPool::Instance() {
    static SqlConnPool connPool;    // 构造实例，且保证仅有一个
    return &connPool;
}

void SqlConnPool::Init(const char* host, int port,  // 主机地址，端口
            const char* user,const char* pwd, const char* dbName,   // 用户，密码，数据库名
            int connSize = 10) {    // 默认的连接数
    assert(connSize > 0);
    for (int i = 0; i < connSize; i++) {
        MYSQL *sql = nullptr;
        sql = mysql_init(sql);  // 数据库初始化
        if (!sql) {
            LOG_ERROR("MySql init error!"); // 初始化不成功则报错
            assert(sql);
        }
        sql = mysql_real_connect(sql, host, // 库函数，该函数进行连接，返回一个指向连接的地址
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
    {
        lock_guard<mutex> locker(mtx_);
        sql = connQue_.front();
        connQue_.pop(); // 使用队列中的一个连接
    }
    return sql; // 返回连接的地址
}

// GetConn()从队列中获取了一个已经建立连接的数据库对象
// 这个数据库连接对象可能需要在系统的其他地方使用，等用完之后
// 需要重新还回来，因此FreeConn函数会检查sql是否为空
// 如果连接还在，那么就要重新放回队列
void SqlConnPool::FreeConn(MYSQL* sql) {
    assert(sql);
    lock_guard<mutex> locker(mtx_);
    connQue_.push(sql); // 将连接压进去
    // 通知其他线程，有一个新的可用连接了
    sem_post(&semId_);
}

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

// 获取空闲的连接数量
int SqlConnPool::GetFreeConnCount() {
    lock_guard<mutex> locker(mtx_);
    return connQue_.size(); // 在队列中的，就代表可以使用的连接数量
}

// 析构
SqlConnPool::~SqlConnPool() {
    ClosePool();
}
