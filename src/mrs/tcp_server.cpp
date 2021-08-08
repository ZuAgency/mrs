#include"tcp_server.h"

//channel
channel::channel(int a_fd, int a_events, event_loop* a_event_loop):
    fd(a_fd), events(a_events), p_event_loop(a_event_loop)
{}

channel::~channel() {}

int channel::read() {
    return 0;
}

int channel::write() {
    return 0;
}

void channel::set_write_event_enable(bool enable){
    if(enable)
        events |= EVENT_WRITE;
    else
        events &= (~EVENT_WRITE);
    
    p_event_loop->update_channel_event(fd, this);
}

bool channel::get_write_event() {
    return events & EVENT_WRITE;
}

int channel::get_fd() {
    return fd;
}

int channel::get_events() {
    return events;
}

event_loop* channel::get_event_loop() {
    return p_event_loop;
}

//wakeup_channel
wakeup_channel::wakeup_channel(int a_fd, int a_events, event_loop* a_event_loop) :
    channel(a_fd, a_events, a_event_loop)
    {
        log_msg("[wakeup channel] created fd = %d\n", a_fd);
    }

int wakeup_channel::read() {
    //读取一个字符，作用是让子线程从dispatch的阻塞中苏醒。
    log_msg("[wakeup channel] wakeup\n");
    char c;
    ssize_t n = ::read(get_event_loop()->get_second_socket_fd(), &c, sizeof(c));
    if(n != sizeof(c))
        log_err("handle wakeup failed");
    //msg "wakeup" p_event_loop->get_name();
    return 0;
}


//connection_channel
connection_channel::connection_channel(int a_fd, int a_events, event_loop* a_event_loop, tcp_connection* a_tcp_connection) :
    channel(a_fd, a_events,a_event_loop), p_tcp_connection(a_tcp_connection)
    {
        log_msg("[connection channel] created fd = %d\n", a_fd);
    }

connection_channel::~connection_channel() {
    log_msg("connect_channel destruction\n");
    delete p_tcp_connection;
}

int connection_channel::read() {
    buffer buf;
    int p = buf.socket_read(get_fd());

    if(!p)
        return 1;

    log_msg("[connect channel] read %d bytes\n", p );
    //log_msg("[connect channel] read %d bytes\n%s\n", p, buf.get_readable_data());
    p_tcp_connection->message(&buf);
    //log_msg("[connect channel] readend\n");
    return 0;
}

//listen_channel
listen_channel::listen_channel(int a_fd, int a_events, event_loop* a_event_loop, TCPserver_base* a_TCPserver) :
    channel(a_fd, a_events, a_event_loop),
    p_TCPserver(a_TCPserver)
{
    log_msg("[listen channel] created fd = %d\n", a_fd);
}

int listen_channel::read() {
    log_msg("[listen channel] read\n");
    return p_TCPserver->handle_connection_established();
}


//channel_map
channel_map::channel_map() : m_map{} {
    /*
        暂时使用map实现
    */
}

channel_map::~channel_map() {}

int channel_map::activate(int fd, int events) {
    // log_msg("[channel map] activate fd = %d\n", fd);
    if(!m_map.count(fd))
        return 0;
    
    channel* cc = m_map[fd];
    assert(fd == cc->get_fd());

    if(events & EVENT_READ) {
        if(cc->read()) {//返回值不为0则出错
            close(fd);
            log_msg("[channel map] fd == %d, closed\n", fd);
            //m_map[fd] = nullptr;
            remove(fd);
            delete cc;
            log_msg("[channel map] now sockets = %d\n", m_map.size());
            return 0;
        }
    }
    if(events & EVENT_WRITE)
        cc->write();
    return 0;
}

int channel_map::insert(int fd, channel* cc) {
    //log_msg("[channel map] insert channel fd = %d\n", cc->get_fd());
    if(m_map.count(fd) && m_map[fd])
        return 0;
    m_map[fd] = cc;
    return 0;
}

channel* channel_map::operator[](int fd) {
    if(!m_map.count(fd))
        return nullptr;
    return m_map[fd];
}

bool channel_map::contains(int fd) {
    return m_map.count(fd);
}

void channel_map::remove(int fd) {
    m_map.erase(fd);
}

//event_dispatcher
event_dispatcher::event_dispatcher(channel_map* a_channel_map) :
    p_channel_map(a_channel_map),
    event_count{},
    nfds{},
    realloc_copy{}
{
    efd = epoll_create1(0);
    if(efd == -1)
        throw std::runtime_error("epoll create failed");
    
    m_events = new epoll_event[MAX_EVENTS];
}

event_dispatcher::~event_dispatcher() {
    delete[] m_events;
    close(efd);
}

int event_dispatcher::add(channel* cc) {
    return do_modify(cc, EPOLL_CTL_ADD);
}

int event_dispatcher::remove(channel* cc) {
    return do_modify(cc, EPOLL_CTL_DEL);
}

int event_dispatcher::update(channel* cc) {
    return do_modify(cc, EPOLL_CTL_MOD);
}

int event_dispatcher::dispatch() {
    //log_msg("count: %d\n", p_channel_map->size());
    int n = epoll_wait(efd, m_events, MAX_EVENTS, -1);
    //msg epoll_wait wakeup thread_name

    decltype(epoll_event::events) events;
    for(int i{}; i < n; ++i) {
        events = m_events[i].events;

        if((events & EPOLLERR) || (events & EPOLLHUP)) {
            log_err("epoll error\n");
            close(m_events[i].data.fd);
            continue;
        }

        if(events & EPOLLIN) {
            //log_msg("[event dispathcer] get message channel fd = %d\n", m_events[i].data.fd);
            p_channel_map->activate(m_events[i].data.fd, EVENT_READ);
        }

        if(events & EPOLLOUT) {
            p_channel_map->activate(m_events[i].data.fd, EVENT_WRITE);
        }

    }
    return 0;
}

int event_dispatcher::do_modify(channel* cc, uint32_t type) {
    int fd = cc->get_fd();
    int events{};

    if(cc->get_events() & EVENT_READ)
        events |= (EPOLLIN | EPOLLET);
    if(cc->get_events() & EVENT_WRITE)
        events |= (EPOLLOUT | EPOLLET);
    
    epoll_event event;
    event.data.fd = fd;
    event.events = events;

    if(epoll_ctl(efd, type, fd, &event) == -1) {
        switch(type) {
        case EPOLL_CTL_ADD:{
            throw std::runtime_error("epoll_ctl add fd failed");
            break;
        }
        case EPOLL_CTL_DEL:{
            throw std::runtime_error("epoll_ctl delete fd failed");
            break;
        }
        case EPOLL_CTL_MOD:{
            throw std::runtime_error("epoll_ctl modify fd failed");
            break;
        }
        default:
            ;
        }
    }
    
    return 0;
}


//acceptor
acceptor::acceptor(int port) :
    listen_port(port)
{
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(listen_fd == -1)
        throw std::runtime_error("socket create failed");
    make_noblocking(listen_fd);

    sockaddr_in server_addr{AF_INET, htons(listen_port), htonl(INADDR_ANY)};

    int on = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    if(bind(listen_fd, pointer_cast<sockaddr*>(&server_addr), sizeof(server_addr)) == -1)
        throw std::runtime_error("bind failed");
    
    if(listen(listen_fd, LISTENQ))
        throw std::runtime_error("listen failed");
    
}

int acceptor::get_listen_fd(){
    return listen_fd;
}

int acceptor::get_port(){
    return listen_port;
}

//channel_element
channel_element::channel_element(channel* a_channel, int a_type, channel_element* a_next) :
    p_channel(a_channel),
    m_type(a_type),
    p_next(a_next)
{}

channel_element::~channel_element(){

}

channel* channel_element::get_channel(){
    return p_channel;
}

int channel_element::type(){
    return m_type;
}

channel_element* channel_element::next(){
    return p_next;
}

void channel_element::set_next(channel_element* a_next){
    p_next = a_next;
}


//event_loop
event_loop::event_loop(const char* a_thread_name) : 
    thread_name(a_thread_name),
    quit{},
    map{},
    dispatcher(&map),
    socket_pair{},
    is_handle_pending{},
    pending_front(nullptr),
    pending_back(nullptr),
    owner_thread_id(pthread_self()),
    mutex{},
    cond{}
{
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cond, NULL);
    /*
        从reactor线程是一个无限循环的event_loop执行体，在没有已注册事件发生
        的情况下，该线程阻塞在event_dispatcher的dispatch函数上。这种情况下如
        何让主线程把已连接套接字交给从reactor线程呢？

        可以令从reactor线程从dispatch函数上返回，再让从reactor线程返回后注册
        新的已连接套接字事件即可。
        构建一个类似管道的描述字，令event_dispatcher注册该描述字，当想让从
        reactor线程苏醒时，向管道上发送一个字符即可。
        socketpair函数创建的套接字对，其作用就是在一侧写时，另一侧就可以感知
        到读的事件。这里也可以直接使用UNIX的pipe管道。
    */
    if(socketpair(AF_UNIX, SOCK_STREAM, 0, socket_pair) == -1)
        log_err("socketpair set failed");

    channel *cc = new wakeup_channel(socket_pair[1], EVENT_READ, this);
    add_channel_event(socket_pair[1], cc);
}

event_loop::~event_loop(){

}

int event_loop::run(){
    /*

    */
    if(owner_thread_id != pthread_self())
        std::exit(1);

    while(!quit){
        log_msg("[%s] loop\n", thread_name);
        //阻塞以等待I/O事件、或其他channel。
        dispatcher.dispatch();
        //处理的当前监听的事件列表。
        handle_pending_channel();
    }

    return 0;
}

int event_loop::add_channel_event(int fd, channel* cc){
    //log_msg("[%s] add event channel fd = %d\n", thread_name, fd);
    return do_channel_event(fd, cc, 1);
}

int event_loop::remove_channel_event(int fd, channel* cc){
    return do_channel_event(fd, cc, 2);
}

int event_loop::update_channel_event(int fd, channel* cc){
    return do_channel_event(fd, cc, 3);
}

void event_loop::assert_in_same_thread(){
    if(owner_thread_id != pthread_self()){
        log_err("not in the same thread");
        exit(-1);
    }
}

int event_loop::get_second_socket_fd(){
    return socket_pair[1];
}

const char* event_loop::get_thread_name(){
    return thread_name;
}

int event_loop::handle_pending_channel(){
    /*
        遍历当前pending的channel_event列表，将它们同event_dispatcher关联起来，从而
        修改感兴趣的事件集合。

        注意当event_loop线程获得活动事件后，会回调事件处理函数，这样像message等应用
        程序代码也会在event_loop线程执行。若此处业务逻辑过于复杂，就会导致
        handle_pending_channel函数执行时间过长，从而影响I/O检测。
        故将I/O线程同业务逻辑线程隔离，令I/O线程只专注于处理I/O交换，业务逻辑线程处理
        业务，是比较常见的做法。
    */
    pthread_mutex_lock(&mutex);
    is_handle_pending = 1;

    for(channel_element* p = pending_front; p != nullptr; p = p->next()){
        channel* cc = p->get_channel();
        switch(p->type()){
        case 1:{
            handle_pending_add(cc);
            break;
        }
        case 2:{
            handle_pending_remove(cc);
            break;
        }
        case 3:{
            handle_pending_update(cc);
            break;
        }
        default:
            ;
        }
    }

    clear_pending_channel();
    
    is_handle_pending = 0;
    pthread_mutex_unlock(&mutex);
    return 0;
}

void event_loop::channel_buffer_nolock(int fd, channel* cc, int type){
    channel_element* ce = new channel_element(cc, type);
    //log_msg("[%s] struct insert channel fd = %d\n", thread_name, fd);
    if(!pending_front)
        pending_front = pending_back = ce;
    else{
        pending_back->set_next(ce);
        pending_back = ce;
    }
}

int event_loop::do_channel_event(int fd, channel* cc, int type){
    /*
        主线程往子线程的数据中增加需要处理的channel event对象，所增加的channel
        对象以链表的形式维护在子线程的数据结构中。
    */
    //log_msg("[%s] do channel event channel fd = %d\n", thread_name, cc->get_fd());
    //上锁
    pthread_mutex_lock(&mutex);

    assert(!is_handle_pending);
    //log_msg("[%s] ready to add channel fd = %d\n", thread_name, cc->get_fd());
    //向该线程的channel列表中增加新的channel
    channel_buffer_nolock(fd, cc, type);

    pthread_mutex_unlock(&mutex);
    //若是主线程发起操作，就需要调用wakeup()唤醒子线程，否则如果是子线程
    //自己操作，就可以直接操作
    if(!is_in_same_thread())
        wakeup();
    else
        handle_pending_channel();
    
    return 0;
}

int event_loop::handle_pending_add(channel* cc){
    int fd = cc->get_fd();
    //msg
    if(fd < 0)
        return 0;

    map.insert(fd, cc);
    dispatcher.add(cc);

    return 0;
    
}

int event_loop::handle_pending_remove(channel* cc){
    int fd = cc->get_fd();
    if(fd < 0)
        return 0;
    
    channel* c = map[fd];
    int retval{};

    if(dispatcher.remove(c))
        retval = -1;
    else
        retval = 1;

    map.remove(fd);

    return retval;
}

int event_loop::handle_pending_update(channel* cc){
    //msg update channel
    int fd = cc->get_fd();
    if(fd < 0)
        return 0;
    if(!map.contains(fd))
        return -1;
    
    dispatcher.update(cc);

    return 0;
}

bool event_loop::is_in_same_thread(){
    return owner_thread_id == pthread_self();
}

int event_loop::wakeup(){
    log_msg("[%s] wakeup\n", thread_name);
    char a = 'a';
    ssize_t n = write(socket_pair[0], &a, sizeof(a));

    if(n != sizeof(a))
        log_err("wakeup event loop thread failed");

    return 0;
}

void event_loop::clear_pending_channel(){
    channel_element* t;
    while(pending_front){
        t = pending_front;
        pending_front = pending_front->next();
        delete t;
    }
    pending_back = nullptr;
}

//event_loop_thread
event_loop_thread::event_loop_thread() :
    m_event_loop{},
    thread_id{},
    mutex{},
    cond{},
    thread{},
    thread_name{}
{
    pthread_mutex_init(&mutex, nullptr);
    pthread_cond_init(&cond, nullptr);
}

event_loop_thread::~event_loop_thread(){
    delete[] thread_name;
    delete m_event_loop;
}

event_loop* event_loop_thread::start(int no){
    /*
        用于主线程创建子线程。子线程一旦创建，就立即执行thread::run，其作用是
        初始化event_loop对象。run在完成后会调用pthread_cond_signal来通知start()中
        被阻塞在pthread_cond_wait中的主线程，这样主线程就会从wait中苏醒，继续执行
        代码。
        子线程本身也通过调用run进入到一个无限循环的事件分发执行体中，等待子线程
        reactor中注册过的事件发生。
    */
    char* name = new char[32];
    sprintf(name, "thread %d\0", no);
    thread_name = name;

    pthread_create(&thread_id,
                    nullptr,
                    [](void* pt)->void* {
                        return reinterpret_cast<::event_loop_thread*>(pt)->run();
                    },
                    this);//

    assert(!pthread_mutex_lock(&mutex));

    while(!m_event_loop)
        assert(!pthread_cond_wait(&cond, &mutex));
    
    assert(!pthread_mutex_unlock(&mutex));
    
    log_msg("[event loop thread] started, %s\n", thread_name);

    return m_event_loop;
}

void* event_loop_thread::run(){
    /*
        初始化event_loop对象，只有在创建完毕后，p_thread_loop才不为空指针。这个变化
        是子线程完成初始化的标志，也是信号量守护的变化。通过使用锁和信号量，解决了
        主线程和子线程同步的问题。
        子线程初始化完成后，主线程才会继续执行。
    */
    pthread_mutex_lock(&mutex);

    m_event_loop = new event_loop(thread_name);
    
    log_msg("[event loop thread] init and signal %s\n", thread_name);

    pthread_cond_signal(&cond);

    pthread_mutex_unlock(&mutex);

    m_event_loop->run();

    return{};
}

event_loop* event_loop_thread::get_event_loop(){
    return m_event_loop;
}

//thread_pool
thread_pool::thread_pool(event_loop* a_main_loop, int a_thread_number) :
    is_started{},
    p_main_loop(a_main_loop),
    thread_number(a_thread_number),
    event_loop_threads(nullptr),
    position{}{

    }

thread_pool::~thread_pool(){
    delete[] event_loop_threads;
}

void thread_pool::start(){
    assert(!is_started);
    p_main_loop->assert_in_same_thread();

    is_started = 1;

    if(thread_number <= 0)
        return;
    
    event_loop_threads = new event_loop_thread[thread_number];
    for(int i{}; i!= thread_number; ++i)
        event_loop_threads[i].start(i);
}

event_loop* thread_pool::get_event_loop(){
    assert(is_started);
    p_main_loop->assert_in_same_thread();

    auto selected = p_main_loop;

    if(thread_number > 0){
        selected = event_loop_threads[position].get_event_loop();
        if(++position >= thread_number)
            position = 0;
    }

    return selected;
}

//buffer
// static const int INIT_BUFFER_SIZE = 1048576;
static const int INIT_BUFFER_SIZE = 65536;

buffer::buffer() :
    data(new char[INIT_BUFFER_SIZE]),
    read_position{},
    write_position{},
    total_size(INIT_BUFFER_SIZE - 1)
{}

buffer::~buffer(){
    delete[] data;
}

int buffer::get_writeable_size(){
    return total_size - write_position;
}

int buffer::get_readable_size(){
    return write_position - read_position;
}

int buffer::get_front_spare_size(){
    return read_position;
}

char* buffer::get_readable_data(){
    return data + read_position;
}

int buffer::append(const void* a_data, int size){
    if(a_data){
        make_room(size);
        memcpy(data + write_position, a_data, size);
        write_position += size;
    }
    return {};
}

int buffer::append_string(const char* s){
    if(s){
        int size = strlen(s);
        append(s, size);
    }
    return {};
}

int buffer::append_char(char c){
    make_room(1);
    data[write_position++] = c;
    return {};
}

char buffer::read_char(){
    char c = data[read_position];
    ++read_position;
    return c;
}  

int buffer::socket_read(int fd){
    char add_buf[INIT_BUFFER_SIZE];
    int max_writeable = get_writeable_size();
    iovec vec[2]{
        {data + write_position, max_writeable},
        {add_buf, sizeof(add_buf)}
    };
    int result = readv(fd, vec, 2);
    if(result < 0)
        return -1;
    else if(result <= max_writeable){
        //
        write_position += result;
    }
    else{
        //
        write_position = total_size;
        append(add_buf, result - max_writeable);
    }
    data[write_position] = 0;
    return result;

}

char* buffer::find_CRLF(){
    void* crlf = memmem(data + read_position, get_readable_size(), "\r\n", 2);
    return pointer_cast<char*>(crlf);
}

int buffer::send(tcp_connection* t){
    int size = get_readable_size();
    int result = t->send_data(data + read_position, size);
    read_position += size;
    //log_msg("[connection] send %d bytes, now buffer size is %d bytes\n", size, total_size);
    return result;
}

void buffer::clear(){
    read_position = 0;
    write_position = 0;
}

auto buffer::capacity(){
    return total_size;
}

void buffer::make_room(int size){
    if(get_writeable_size() >= size)
        return;
    
    int readable_size = get_readable_size();
    if(get_front_spare_size() + get_writeable_size() >= size){//当前空余空间足够
        
        // if(read_position > readable_size)
        //     memcpy(data, data + read_position, readable_size);
        // else
        for(int i{}; i!= readable_size; ++i)
            memcpy(data + i, data + read_position + i, i);
            
    }
    else{
        char* new_space = new char[total_size + size + 1];
        memcpy(new_space, data + read_position, readable_size);
        delete data;

        data = new_space;
        total_size += size;
    }

    read_position = 0;
    write_position = readable_size;

}

//tcp_connection
tcp_connection::tcp_connection(int connect_fd, event_loop* a_event_loop) :
                p_event_loop(a_event_loop),
                input_buffer(new buffer),
                output_buffer(new buffer)
{
    /*
        tcp_connection代表一个连接，构造意味着连接建立。
        主要操作是创建一个channel对象，然后把该对象注册到event_loop中。
    */
    name = new char[32];
    sprintf(name, "connection-%d\0", connect_fd);

    m_channel = new connection_channel(connect_fd, EVENT_READ, p_event_loop, this);

    p_event_loop->add_channel_event(connect_fd, m_channel);
}

tcp_connection::~tcp_connection(){
    //delete m_channel;
    delete[] name;

    delete output_buffer;
    delete input_buffer;
}

int tcp_connection::message(buffer* buf){
    return 0;
}

int tcp_connection::write_completed(){
    return 0;
}

int tcp_connection::connection_closed(){
    return 0;
}

int tcp_connection::send_data(const void* data, int size){
    size_t nwrited{},
        nleft(size),
        fault{};
    
    if(!m_channel->get_write_event() && !output_buffer->get_readable_size()){
        nwrited = write(m_channel->get_fd(), data, size);
        if(nwrited >= 0)
            nleft -= nwrited;
        else{
            nwrited = 0;
            if(errno != EWOULDBLOCK){
                if(errno == EPIPE || errno == ECONNRESET)
                    fault = 1;
            }
        }
    }

    if(!fault && nleft > 0){
        output_buffer->append(const_pointer_cast<char*>(data) + nwrited, nleft);
        if(!m_channel->get_write_event())
            m_channel->set_write_event_enable(1);
    }

    return nwrited;
}

void tcp_connection::shutdown_connection(){
    if(shutdown(m_channel->get_fd(), SHUT_WR) < 0)
        log_msg("[tcp connection] shutdown failed, socket == %d\n", m_channel->get_fd());
}
