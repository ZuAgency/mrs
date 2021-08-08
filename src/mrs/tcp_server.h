#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include"common.h"

const int EVENT_READ = 0x2;
const int EVENT_WRITE = 0x4;

class event_loop;
class tcp_connection;
class TCPserver_base;


class channel{
/*
    用来同event_dispatcher交互的结构体
    抽象了事件的分发
*/
public:
    channel(int a_fd, int a_events, event_loop* a_event_loop);
    
    virtual ~channel();

    virtual int read();

    virtual int write();

    void set_write_event_enable(bool enable);

    bool get_write_event();

    int get_fd();

    int get_events();

    event_loop* get_event_loop();

private:
    int fd;
    int events;
    event_loop* p_event_loop;
};

class wakeup_channel : public channel{
public:
    wakeup_channel(int a_fd, int a_events, event_loop* a_event_loop);

    int read() override;
};

class connection_channel : public channel{
/*
    该channel用于建立新连接。
*/
public:
    connection_channel(int a_fd, int a_events, event_loop* a_event_loop, tcp_connection* a_tcp_connection);
    
    ~connection_channel();

    int read() override;

private:
    tcp_connection* p_tcp_connection;
};

class listen_channel : public channel{
/*
    作用是监听端口，当有客户端请求时，调用服务器建立连接函数。
*/
public:
    listen_channel(int a_fd, int a_events, event_loop* a_event_loop, TCPserver_base* a_TCPserver);

    int read() override;

private:
    TCPserver_base* p_TCPserver;
};

class channel_map{
public:
    channel_map();

    ~channel_map();

    int activate(int fd, int events);

    int insert(int fd, channel* cc);

    channel* operator[](int fd);

    bool contains(int fd);

    void remove(int fd);

private:
    std::map<int, channel*> m_map;
};

const int MAX_EVENTS = 128;

class event_dispatcher{
public:
    event_dispatcher(channel_map* a_channel_map);

    ~event_dispatcher();

    int add(channel* cc);

    int remove(channel* cc);

    int update(channel* cc);

    int dispatch();
    
private:
    int do_modify(channel* cc, uint32_t type);

    channel_map* p_channel_map;
    int event_count;
    int nfds;
    int realloc_copy;
    int efd;
    epoll_event* m_events;
};

const int LISTENQ = 1024;

class acceptor{
/*
    主reactor线程，一旦建立就会以event_loop的形式阻塞在event_dispatcher的dispatch上。
    实际上，它在等待监听套接字上的事件发生，即已完成连接。一旦有连接完成，就会创建出
    连接对象tcp_connection，以及channel对象。

    当用户使用多个从reactor线程时，主线程会创建多个子线程，每个子线程创建后按照主线程
    指定的启动函数运行，并进行初始化，然而主线程如何判断子线程已经初始化并启动是个问题

    在设置了多个线程的情况下，需要将新创建的以连接套接字对应的读写时间交给一个从reactor
    线程。
*/
public:
    acceptor(int port);

    int get_listen_fd();

    int get_port();

private:
    int listen_port;
    int listen_fd;
};

class channel_element{
public:
    channel_element(channel* a_channel, int a_type, channel_element* a_next = nullptr);

    ~channel_element();
    
    channel* get_channel();
    
    int type();
    
    channel_element* next();
    
    void set_next(channel_element* a_next);

private:
    channel* p_channel;
    int m_type;//1:add;2:delete
    channel_element* p_next;
};

class event_loop{
/*
    整个反应堆模式的核心，event_dispatcher的作用是等待事件的发生。
*/
public:
    event_loop(const char* a_thread_name = "main thread");

    ~event_loop();

    int run();

    int add_channel_event(int fd, channel* cc);

    int remove_channel_event(int fd, channel* cc);

    int update_channel_event(int fd, channel* cc);

    void assert_in_same_thread();

    int get_second_socket_fd();
    
    const char* get_thread_name();

private:
    int handle_pending_channel();

    void channel_buffer_nolock(int fd, channel* cc, int type);
    
    int do_channel_event(int fd, channel* cc, int type);
    
    int handle_pending_add(channel* cc);
    
    int handle_pending_remove(channel* cc);
    
    int handle_pending_update(channel* cc);
    
    bool is_in_same_thread();
    
    int wakeup();

    void clear_pending_channel();

    const char* thread_name;
    int quit;
    channel_map map;
    event_dispatcher dispatcher;

    int socket_pair[2];//父线程用来通知子线程有新的事件需要处理。
    //保留在子线程内需要处理的新事件。
    int is_handle_pending;
    channel_element* pending_front;
    channel_element* pending_back;

    pthread_t owner_thread_id;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
};

class event_loop_thread{
public:
    event_loop_thread();
    
    ~event_loop_thread();
    
    event_loop* start(int no);
    
    void* run();
    
    event_loop* get_event_loop();

private:
    event_loop* m_event_loop;
    pthread_t thread_id;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int thread;
    const char* thread_name;
};

class thread_pool{
/*
    线程池，从reactor线程。
*/
public:
    thread_pool(event_loop* a_main_loop, int a_thread_number);

    ~thread_pool();

    void start();

    event_loop* get_event_loop();

private:
    bool is_started;
    event_loop* p_main_loop;//创建该thread_pool的主线程。
    int thread_number;//线程数
    event_loop_thread* event_loop_threads;//数组指针，指向创建的event_loop_thread数组
    int position;//表示在数组里的位置，用来决定选择哪个event_loop_thread服务
};

class buffer{
public:
    buffer();
    
    ~buffer();
    
    int get_writeable_size();

    int get_readable_size();

    int get_front_spare_size();

    char* get_readable_data();

    int append(const void* a_data, int size);

    int append_string(const char* s);

    int append_char(char c);

    char read_char();

    int socket_read(int fd);

    char* find_CRLF();

    int send(tcp_connection* t);

    void clear();
    
    auto capacity();

private:
    void make_room(int size);

    char* data;
    int read_position;  //读取位置
    int write_position; //写入位置
    int total_size;
};

class tcp_connection{
public:
    tcp_connection(int connect_fd, event_loop* a_event_loop);

    tcp_connection(const tcp_connection& r) = delete;

    ~tcp_connection();
    //buffer读取到数据时调用
    virtual int message(buffer* buf);
    //buffer写完数据后调用
    virtual int write_completed();
    //连接关闭后调用
    virtual int connection_closed();

    int send_data(const void* data, int size);
    
    void shutdown_connection();
protected:
    event_loop* p_event_loop;
    channel* m_channel;
    buffer* input_buffer;
    buffer* output_buffer;
    char* name;
};

class TCPserver_base{
public:
    TCPserver_base(){}

    TCPserver_base(const TCPserver_base&) = delete;

    virtual ~TCPserver_base(){}
    
    virtual void start() = 0;

    virtual int handle_connection_established() = 0;

};

template<typename Connection_Type>
class TCPserver : public TCPserver_base{
public:
    TCPserver(int port, int a_thread_num ) :
        main_event_loop{},
        m_acceptor(port),
        thread_num(a_thread_num),
        m_thread_pool(&main_event_loop, thread_num)
    {}
    
    ~TCPserver(){}
    
    void start() override{
        /*
            开启监听。
        */
        //开启多个线程
        m_thread_pool.start();

        int listen_fd = m_acceptor.get_listen_fd();

        channel* cc = new listen_channel(listen_fd, EVENT_READ, &main_event_loop, this);

        main_event_loop.add_channel_event(listen_fd, cc);

        return;
    }

    int handle_connection_established() override{
        /*
            当出现了新的连接，主线程需要调用该函数。
        */
        int listen_fd = m_acceptor.get_listen_fd();

        sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int connect_fd = accept(listen_fd, pointer_cast<sockaddr*>(&client_addr), &client_len);
        if(connect_fd == -1)
            throw std::runtime_error("connect failed");
        make_noblocking(connect_fd);

        //msg "new connection established, socket:", connect_fd

        //从线程池中选择一个event_loop来服务这个新的连接套接字，
        //并为其创建一个tcp_connection对象
        tcp_connection* connection = new Connection_Type(connect_fd, m_thread_pool.get_event_loop());
        

        return 0;
    }

    int run(){
        return main_event_loop.run();
    }
private:
    event_loop main_event_loop;
    acceptor m_acceptor;

    int thread_num;
    thread_pool m_thread_pool;//从reactor线程线程池
};

#endif
