#ifndef BUFFER_H
#define BUFFER_H

#include <cstring>
#include <iostream>
#include <unistd.h>
#include <sys/uio.h>
#include <vector>
//提供了一组原子操作，用于保证在多线程环境下对单个数据的访问是原子的
//即不可分割的。这可以避免数据竞争和保证线程安全。
#include <atomic>
#include <assert.h>

class Buffer
{
private:
    char* BeginPtr_();
    const char* BeginPtr_() const;
    void MakeSpace(size_t len);

    //底层是一个动态扩容的一维数组，
    //二是因为当缓冲区大小不够时可以进行扩容
    std::vector<char> buffer_;
    std::atomic<std::size_t> readPos_;
    std::atomic<std::size_t> writePos_;
public:
    Buffer(int initBufferSize=1024);
    //告诉编译器生成一个简单的构造函数，这个构造函数不执行任何特定初始化，
    //因此适用于当类中没有复杂的资源分配时。
    ~Buffer()=default;

    //可写入数据，往Buffer中写入的数据都是往这里写的
    size_t WritableBytes() const;
    //可读数据，从Buffer中读取的数据都是从这里读取的
    size_t ReadableBytes() const;
    //预留空间
    size_t PrependableBytes() const;

    const char* Peek() const;
    void EnsureWriteable(size_t len);
    void HasWritten(size_t len);

    //当你从缓冲区中读取了数据后，需要挪动readerIndex就调用此函数
    void Retrieve(size_t len);
    void RetireveUntil(const char* end);

    void RetrieveAll();
    std::string RetrieveAllToStr();

    const char* BeginWriteConst() const;
    char* BeginWrite();

    void Append(const std::string& str);
    void Append(const char* str,size_t len);
    void Append(const void* data,size_t len);
    void Append(const Buffer& buff);

    ssize_t ReadFd(int fd,int* Errno);
    ssize_t WriteFd(int fd,int* Errno);
};




#endif
