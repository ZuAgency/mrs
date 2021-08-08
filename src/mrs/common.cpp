#include"common.h"

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