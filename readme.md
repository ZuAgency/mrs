# 简介

一个多线程TCP服务器框架  
基于linux，使用c++11标准  

程序所参考使用的项目：  
https://github.com/froghui/yolanda  
https://github.com/qicosmos/cinatra  
https://github.com/oatpp/oatpp  

参考网站：  
https://zh.cppreference.com/  
https://man7.org/linux/man─pages/  

文件结构：  
```
mrs/
├── src
│   ├── mrs
│   │   ├── third
│   │   │   ├── picohttpparser.h
│   │   │   └── picohttpparser.c
│   │   ├── common.h
│   │   ├── common.cpp
│   │   ├── tcp_server.h
│   │   ├── tcp_server.cpp
│   │   ├── http_server.h
│   │   └── http_server.cpp
│   ├── mrs.h
│   └── content_type.h
├── makefile
├── main.cpp
└── readme.md
```