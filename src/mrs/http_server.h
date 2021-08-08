#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include"common.h"
#include"tcp_server.h"
#include"picohttpparser.h"

//http-request

class http_request{
public:
    http_request();

    ~http_request();

    void reset();

    const char* get_header(const char* key);

    int is_parsed();

    int close_connection();

    int parse_http_request(buffer* input);

    char* get_url();

    char* get_path();

private:
    void clear_free_space();

    char* version;
    char* method;
    char* url;
    char* path;
    int current_state;
    struct request_header{
        char* key;
        char* value;
    }* request_headers;
    int request_headers_number;
};

//http-response

struct response_header{
    char* key;
    char* value;
};

enum http_statuscode{
    unknown,
    ok = 200,
    moved_permanently = 301,
    bad_request = 400,
    not_found = 404,
};

class http_response{
public:
    http_response();

    ~http_response();

    void encode_buffer(buffer* output);

    virtual int request(http_request* a_http_request);

protected:
    http_statuscode status;
    const char* status_message;
    const char* content_type;
    buffer body;
    response_header* response_headers;
    int response_headers_number;
    int keep_connected;
};

template<typename RESPONSE>
class http_connection : public tcp_connection{
public:
    http_connection(int connect_fd, event_loop* a_event_loop) :
        tcp_connection(connect_fd, a_event_loop),
        m_http_request{}
    {}

    int message(buffer* buf)override {
        //log_msg("[http connection] get message from tcp connection %s\n", name);
        if(m_http_request.parse_http_request(buf) == 0){
            const char* error_response = "HTTP/1.1 400 Bad Request\r\n\r\n";
            send_data(error_response, sizeof(error_response));
            shutdown_connection();
        }

        if(m_http_request.is_parsed() == 1){
            RESPONSE response;

            response.request(&m_http_request);

            buffer t_buffer;
            
            response.encode_buffer(&t_buffer);
            //log_msg("[response] encode\n%.*s\n", 500 /*t_buffer.get_readable_size()*/, t_buffer.get_readable_data());
            t_buffer.send(this);

            if(m_http_request.close_connection())
                shutdown_connection();

            m_http_request.reset();
        }
        return 0;
    }

    ~http_connection(){
        //
        //log_msg("[http connection] destructed\n");
    }
    
    int write_completed(){
        //
        return 0;
    }
private:
    http_request m_http_request;
};

#endif
