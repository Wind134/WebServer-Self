/*
- 这是一个数据库连接池
- 数据库中应该是用户的登录信息
*/
#ifndef SQLCONNPOOL_H
#define SQLCONNPOOL_H

#include <mysql/mysql.h>
#include <string>
#include <queue>
#include <mutex>
#include <semaphore.h>
#include <thread>
#include "../log_system/log.h" // 调用了log文件

class SqlConnPool {
public:
    // 获取一个数据库连接池实例，确保只有一个池子
    static SqlConnPool *Instance();

    MYSQL *GetConn();   // 从连接池中获取一个连接，返回类型是一个指向数据库连接的指针
    void FreeConn(MYSQL * conn);    // 释放一个连接，并放回连接池以供重用
    int GetFreeConnCount(); // 获取空闲的数据库连接池数量

    // 执行初始化功能
    void Init(const char* host, int port,
              const char* user,const char* pwd, 
              const char* dbName, int connSize);
    // 关闭整个连接池          
    void ClosePool();

private:
    // 构造与析构设计为私有的是为了配合上述的单例模式，通过设计为私有
    // 外部无法直接创建该类的对象，可以通过上述的Instance函数去创建一个新的连接池实例
    SqlConnPool();
    ~SqlConnPool();

    int MAX_CONN_;  // 最大连接数
    int useCount_;  // 正在连接数
    int freeCount_; // 空闲连接数

    std::queue<MYSQL *> connQue_;   // 保存数据库连接的队列，队列元素类型为指向数据库的指针
    std::mutex mtx_;
    sem_t semId_;
};


#endif // SQLCONNPOOL_H
