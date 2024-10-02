#include "sqlconnpool.h"
using namespace std;

SqlConnPool::SqlConnPool(){
    useCount_=0;
    freeCount_=0;
}

SqlConnPool* SqlConnPool::Instance(){
    //静态变量将在程序的整个运行周期内保持其状态，并且只会初始化一次，
    //确保在多次调用 Instance() 函数时返回同一个对象。
    static SqlConnPool connPool;

    return &connPool;
}

void SqlConnPool::init(const char* host,int port,const char* user,
              const char* pwd,const char* dbName,int connSize){
                assert(connSize>0);
                for(int i=0;i<connSize;i++){
                    MYSQL* sql=nullptr;
                    sql=mysql_init(sql);
                    if(!sql){
                        //LOG_ERROR("MySql init error!");
                        assert(sql);
                    }
                    sql=mysql_real_connect(sql,host,user,pwd,dbName,port,nullptr,0);
                    if(!sql){
                        //LOG_ERROR("MySql init error!");
                    }
                    connQue_.push(sql);
                }
                MAX_CONN_=connSize;
                sem_init(&semId,0,MAX_CONN_);
            }

MYSQL* SqlConnPool::GetConn(){
    MYSQL* sql=nullptr;
    if(connQue_.empty()){
        //LOG_WARN("SqlConnPool busy!");
        return nullptr;
    }
    sem_wait(&semId);
    {
        //保护对 connQue_ 的访问。lock_guard 会在对象生命周期结束时自动释放锁，
        //确保不会发生死锁。
        lock_guard<mutex> locker(mtx);
        //获取第一个连接后删除第一个连接，保证其他线程无法使用这个连接
        sql=connQue_.front();
        connQue_.pop();
    }
    return sql;
}

void SqlConnPool::FreeConn(MYSQL* sql){
    assert(sql);
    lock_guard<mutex> locker(mtx);
    connQue_.push(sql);
    //调用 sem_post 函数释放信号量 semId，这表示现在有一个可用的连接可以被获取。
    //它可能会唤醒一个正在等待连接的线程。
    sem_post(&semId);
}

void SqlConnPool::ClosePool(){
    lock_guard<mutex> locker(mtx);
    while(!connQue_.empty()){
        auto item=connQue_.front();
        connQue_.pop();
        mysql_close(item);
    }
}

int SqlConnPool::GetFreeConnCount(){
    lock_guard<mutex> locker(mtx);
    return connQue_.size();
}

SqlConnPool::~SqlConnPool(){
    ClosePool();
}
