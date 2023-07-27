/*
RAII的基本思想是，通过在对象的构造函数中获取资源，并在对象的析构函数中释放资源，以确保资源在对象生命周期结束时得到正确释放。
这种技术的核心概念是资源的获取与初始化是紧密相关的，它利用了C++编程语言中的对象生命周期管理机制。
*/

#ifndef SQLCONNRAII_H
#define SQLCONNRAII_H
#include "sqlconnpool.h"

/* 资源在对象构造初始化 资源在对象析构时释放*/
class SqlConnRAII {
public:

    // 通传入指向指针的指针，可以改变外部传入的指针值，从而将数据库连接传递给外部调用者
    // 说人话就是*sql的值由于 *sql = connpool->GetConn()，会发生改变，而我们传入的是
    // 指针的指针，那么在这个类外部通过调用*sql，就能获得我们从connpool队列取出来的连接
    // 而如果传入的是指针，只不过是简单的值复制，达不到这个效果，传进去之后获取的还是原来的
    SqlConnRAII(MYSQL** sql, SqlConnPool *connpool) {
        assert(connpool);
        *sql = connpool->GetConn();
        sql_ = *sql;
        connpool_ = connpool;
    }
    
    ~SqlConnRAII() {
        if(sql_) { connpool_->FreeConn(sql_); }
    }
    
private:
    MYSQL *sql_;            // 指向sql连接的指针
    SqlConnPool* connpool_; // 指向数据库连接池的指针
};

#endif //SQLCONNRAII_H