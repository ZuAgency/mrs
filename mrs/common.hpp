#ifndef COMMON_HPP
#define COMMON_HPP

#include<stdexcept>
#include<map>

#include<cstdlib>//exit()
#include<cassert>//assert()
#include<cstdio>//sprintf()
#include<cstdarg>//va_list
#include<cstring>//memcpy()
#include<type_traits>

#include<pthread.h>
#include<sys/socket.h>//socketpair()
#include<unistd.h>//read()
#include<netinet/in.h>//sockaddr_in
#include<fcntl.h>//fcntl()
#include<sys/epoll.h>//epoll()
#include<sys/stat.h>//stat()

template<typename DEST_TYPE, typename SOURCE_TYPE>
DEST_TYPE pointer_cast(SOURCE_TYPE source_type){
    void* t = source_type;
    return static_cast<DEST_TYPE>(t);
}

//底层const指针间的转换
template<typename DEST_TYPE, typename SOURCE_TYPE>
auto const_pointer_cast(SOURCE_TYPE source_type){
    const void* t = source_type;
    return static_cast<const typename std::remove_pointer<DEST_TYPE>::type*>(t);
}

void make_noblocking(int fd){
    fcntl(fd, F_SETFL, O_NONBLOCK);
}

void log_msg(/*const char* lc,*/ const char* fmt, ...){
    va_list ps;
    va_start(ps, fmt);
    vprintf(fmt, ps);
    va_end(ps);
}

void log_err(const char* fmt, ...){
    va_list ps;
    va_start(ps, fmt);
    vfprintf(stderr, fmt, ps);
    va_end(ps);
}

const char* get_extension(const char* file_name){
    if(!file_name)
        return nullptr;
    auto length = strlen(file_name);
    for(auto i = length; i--;){
        if(file_name[i] == '.')
            return file_name + i;
    }
    return nullptr;
}

#endif
