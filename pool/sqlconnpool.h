#ifndef SQLCONNPOOL_H
#define SQLCONNPOOL_H

#include <mysql/mysql.h>
#include <string>
#include <queue>
#include <mutex>
#include <semaphore.h>
#include <thread>
#include <assert.h>


class SqlConnPool{
public:
    static SqlConnPool* Instance();

    MYSQL *GetConn();
    void FreeConn(MYSQL* conn);//将使用完毕的连接归还给连接池
    int GetFreeConnCount();

    void init(const char* host,int port,const char* user,
              const char* pwd,const char* dbName,int connSize);
    void ClosePool();//关闭所有连接并清理资源
private:
    SqlConnPool();
    ~SqlConnPool();

    int MAX_CONN_;//最大连接数
    int useCount_;//正在使用的连接数
    int freeCount_; //当前空闲的连接数
    
    std::queue<MYSQL*> connQue_;
    std::mutex mtx;
    sem_t semId;//信号量，用于控制并发获取连接的数量
};


#endif
