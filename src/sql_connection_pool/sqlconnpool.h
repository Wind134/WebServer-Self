/*
- 这是一个数据库连接池
- 数据库中应该是用户与数据库的连接信息
- 也就是网页中的登录的账户信息
- 这个数据库连接池主要还是为了在网页中实现多用户的注册与登录
*/
#ifndef SQLCONNPOOL_H
#define SQLCONNPOOL_H

#include <mysql/mysql.h>
#include <string>
#include <queue>
#include <mutex>
#include <semaphore.h>  // 调用信号量机制
#include <thread>
#include "../log_system/log.h" // 调用了log文件

class SqlConnPool {
public:
    static SqlConnPool *Instance();

    MYSQL *GetConn();   // 从连接池中获取一个连接，返回类型是一个指向数据库连接的指针
    void FreeConn(MYSQL * conn);    // 释放一个连接，并放回连接池以供重用
    int GetFreeConnCount(); // 获取空闲的数据库连接池数量

    void Init(const char* host, int port,
              const char* user,const char* pwd, 
              const char* dbName, int connSize);

    void ClosePool();

private:

    SqlConnPool();
    ~SqlConnPool();

    int MAX_CONN_;  // 最大连接数
    int useCount_;  // 正在连接数
    int freeCount_; // 空闲连接数

    std::queue<MYSQL *> connQue_;   // 保存数据库连接的队列，队列元素类型为指向数据库连接的指针
    std::mutex mtx_;
    sem_t semId_;
};

#endif // SQLCONNPOOL_H
