#ifndef COMMON_H
#define COMMON_H

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

void make_noblocking(int fd);

void log_msg(/*const char* lc,*/ const char* fmt, ...);

void log_err(const char* fmt, ...);

const char* get_extension(const char* file_name);

#endif
