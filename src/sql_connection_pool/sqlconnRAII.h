/*
RAII的基本思想：
- 通过在对象的构造函数中获取资源，并在对象的析构函数中释放资源，以确保资源在对象生命周期结束时得到正确释放。
- 这种技术的核心概念是资源的获取与初始化是紧密相关的，它利用了C++编程语言中的对象生命周期管理机制。

下面有一些个人理解：
- 同时这个RAII对象可以创建多个，从而也能实现多个连接池的创建(能吗？这个结论或许还有点问题)；
- 但是为何不直接在连接池创建多个呢？不考虑实现单例模式；
*/
#ifndef SQLCONNRAII_H
#define SQLCONNRAII_H
#include "sqlconnpool.h"

/* 资源在对象构造初始化 资源在对象析构时释放*/
class SqlConnRAII {
public:
    /**
     * @brief 构造函数，初始化指向内存池的指针，以及获取一个数据库连接；
     * @param sql 指向数据库连接指针的指针；
     * @param connpool 指向数据库连接池的指针；
     */
    SqlConnRAII(MYSQL** sql, SqlConnPool *connpool) {   // 使用指针的指针参数有一项好处就是，可以让参数中的sql指向的指针也获得连接
        assert(connpool);                               // 如果只是传指针，那么指针本身作为仅作为值传递，影响不到作用域外部的那个变量
        *sql = connpool->GetConn();
        sql_ = *sql;
        connpool_ = connpool;
    }
    
    /**
     * @brief 析构做的事是释放一个连接，也就是说重新放回连接队列，因为已经用完了；
     */
    ~SqlConnRAII() {
        if(sql_) { connpool_->FreeConn(sql_); }
    }
    
private:
    MYSQL *sql_;            // 指向sql连接的指针
    SqlConnPool* connpool_; // 指向数据库连接池的指针(定义指针可以，但是定义对象不行，因为私有)
};

#endif //SQLCONNRAII_H