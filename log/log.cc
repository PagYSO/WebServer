#include "log.h"

using namespace std;

Log::Log(){
    lineCount_=0;
    isAsync_=0;
    writeThread_=0;
    deque_=nullptr;
    toDay_=0;
    fp_=nullptr;
}

Log::~Log(){
    //joinable:检查一个线程是否可以加入
    if(writeThread_ && writeThread_->joinable()){
        while(!deque_->empty()){
            deque_->flush();
        }
        deque_->Close();
        writeThread_->join();
    }
    if(fp_){
        lock_guard<mutex> locker(mtx_);
        flush();
        fclose(fp_);
    }
}

int Log::GetLevel(){
    lock_guard<mutex> locker(mtx_);
    return level_;
}

void Log::SetLevel(int level){
    //日志等级低于level会被忽略
    lock_guard<mutex> locker(mtx_);
    level_=level;
}

void Log::init(int level=1,const char* path,const char* suffix,
    int maxQueueSize){
        isOpen_=true;
        level_=level;
        if(maxQueueSize>0){
            isAsync_=true;
            if(!deque_){
                unique_ptr<BlockDeque<std::string>> newDeque(new BlockDeque<std::string>);
                deque_=move(newDeque);

                std::unique_ptr<std::thread> NewThread(new thread(FlushLogThread));
                writeThread_=move(NewThread);
            }
        }
        else{
            isAsync_=false;
        }
        lineCount_=0;

        time_t timer=time(nullptr);
        struct tm *sysTime=localtime(&timer);
        struct tm t=*sysTime;
        path_=path;
        suffix_=suffix;
        char fileName[LOG_NAME_LEN]={0};
        //1.snprintf会自动在格式化后的字符串尾添加\0，结尾符是包含在size长度内部的。
        //2.snprintf会在结尾加上\0，不管buf空间够不够用，所以不必担心缓冲区溢出。
        //3.snprintf的返回值n，当调用失败时，n为负数，当调用成功时，
        //n为格式化的字符串的总长度（不包括\0），当然这个字符串有可能被截断，因为buf的长度不够放下整个字符串。
        snprintf(fileName,LOG_NAME_LEN-1,"%s%04d_%02d_%02d%s",path_,t.tm_year+1900,t.tm_mon+1,t.tm_mday,
            suffix_);
        toDay_=t.tm_mday;

        {
            lock_guard<mutex>locker(mtx_);
            buff_.RetrieveAll();
            if(fp_){
                flush();
                fclose(fp_);
            }

            fp_=fopen(fileName,"a");
            if(fp_==nullptr){
                mkdir(path_,0777);
                fp_=fopen(fileName,"a");
            }
            assert(fp_!=nullptr);
        }

    }

void Log::write(int level,const char* format,...){
    struct timeval now={0,0};
    //使用gettimeofday()来查看当前时间
    gettimeofday(&now,nullptr);
    time_t tSec=now.tv_sec;
    struct tm *sysTime=localtime(&tSec);
    struct tm t=*sysTime;
    va_list vaList;

    if(toDay_!=t.tm_mday || (lineCount_ && MAX_LINES==0)){
        unique_lock<mutex> locker(mtx_);
        locker.unlock();

        char newFile[LOG_NAME_LEN];
        char tail[36]={0};
        snprintf(tail, 36, "%04d_%02d_%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);

        if(toDay_!=t.tm_mday){
            snprintf(newFile, LOG_NAME_LEN-72,"%s/%s%s",path_,tail,suffix_);
            toDay_=t.tm_mday;
            lineCount_=0;
        }
        else{
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s-%d%s", path_, tail, (lineCount_  / MAX_LINES), suffix_);
        }

        locker.lock();
        flush();
        fclose(fp_);
        fp_=fopen(newFile,"a");
        assert(fp_!=nullptr);
    }
    {
        unique_lock<mutex> locker(mtx_);
        lineCount_++;
        int n = snprintf(buff_.BeginWrite(), 128, "%d-%02d-%02d %02d:%02d:%02d.%06ld ",
                    t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                    t.tm_hour, t.tm_min, t.tm_sec, now.tv_usec);
                    
        buff_.HasWritten(n);
        AppendLogLevelTitle_(level);

        va_start(vaList, format);
        int m = vsnprintf(buff_.BeginWrite(), buff_.WritableBytes(), format, vaList);
        va_end(vaList);

        buff_.HasWritten(m);
        buff_.Append("\n\0", 2);

        if(isAsync_ && deque_ && !deque_->full()) {
            deque_->push_back(buff_.RetrieveAllToStr());
        } else {
            fputs(buff_.Peek(), fp_);
        }
        buff_.RetrieveAll();
    }
}

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

void Log::flush() {
    if(isAsync_) { 
        deque_->flush(); 
    }
    //刷新流 stream 的输出缓冲区
    //如果成功，该函数返回零值。
    //如果发生错误，则返回 EOF，且设置错误标识符（即 feof）
    fflush(fp_);
}

void Log::AsyncWrite_() {
    string str = "";
    while(deque_->pop(str)) {
        lock_guard<mutex> locker(mtx_);
        fputs(str.c_str(), fp_);
    }
}

Log* Log::Instance() {
    static Log inst;
    return &inst;
}

void Log::FlushLogThread() {
    Log::Instance()->AsyncWrite_();
}

