#ifndef HTTP_SERVER_HPP
#define HTTP_SERVER_HPP

#include"common.hpp"
#include"tcp_server.hpp"

//http-request
const int INIT_REQUEST_HEADER_SIZE = 128;
const char* HTTP10 = "HTTP/1.0";
const char* HTTP11 = "HTTP/1.1";
const char* KEEP_ALIVE = "Keep-Alive";
const char* CLOSE = "close";

enum http_request_state{
    REQUEST_STATUS,
    REQUEST_HEADERS,
    REQUEST_BODY,
    REQUEST_DONE
};

class http_request{
public:
    http_request() :
        version(nullptr),
        method(nullptr),
        url(nullptr),
        path(nullptr),
        current_state(REQUEST_STATUS),
        request_headers(new request_header[INIT_REQUEST_HEADER_SIZE]),
        request_headers_number(0)
    {}

    ~http_request(){
        if(request_headers){
            for(int i{}; i!=request_headers_number; ++i){
                delete[] request_headers[i].key;
                delete[] request_headers[i].value;
            }
            delete[] request_headers;
        }
        clear_free_space();
    }

    void reset(){
        clear_free_space();
        current_state = REQUEST_STATUS;
        request_headers_number = 0;
    }

    void add_header(char* key, char* value){
        request_headers[request_headers_number++] = {key, value};
    }

    char* get_header(char* key){
        if(request_headers){
            for(int i{}; i != request_headers_number; ++i)
                if(strncmp(request_headers[i].key, key, strlen(key)) == 0)
                    return request_headers[i].value;
        }
        return nullptr;
    }

    http_request_state get_current_state(){
        return current_state;
    }

    int close_connection(){
        char* connection = get_header("Connection");

        if(connection && strncmp(connection, CLOSE, strlen(CLOSE)) == 0)
            return 1;

        if(version && strncmp(version, HTTP10, strlen(HTTP10)) == 0 &&
            strncmp(connection, KEEP_ALIVE, strlen(KEEP_ALIVE)) == 1){
                return 1;
            }
        return 0;
    }

    int process_status_line(char* start, char* end){
        clear_free_space();
        int size = end -start;
        //method
        log_msg("[process]");
        char* space = pointer_cast<char*>(memmem(start, size, " ", 1));
        assert(space != nullptr);
        int method_size = space - start;
        method = new char[method_size + 1];
        strncpy(method, start, method_size);
        method[method_size] = '\0';

        //url
        start = space + 1;
        space = pointer_cast<char*>(memmem(start, end - start, " ", 1));
        assert(space != nullptr);
        int url_size = space - start;
        url = new char[url_size + 1];
        strncpy(url, start, space - start);
        url[url_size] = '\0';

        //version
        start = space + 1;
        int version_size = end - start;
        version = new char[version_size + 1];
        strncpy(version, start, version_size);
        version[version_size] = '\0';
        assert(space != nullptr);

        return size;
    }

    int parse_http_request(buffer* input){
        int ok = 1;
        while(current_state != REQUEST_DONE){
            if(current_state == REQUEST_STATUS){
                char* crlf = input->find_CRLF();
                if(crlf){
                    int request_line_size = process_status_line(input->get_readable_data(), crlf);
                   if(request_line_size){
                        input->move_read_position_by(request_line_size + 2);
                        //               request line     crlf
                        current_state = REQUEST_HEADERS;
                    }
                }
            }
            else if(current_state == REQUEST_HEADERS){
                char* crlf = input->find_CRLF();
                if(crlf){
                    char* start = input->get_readable_data();
                    int request_line_size = crlf - start;
                    char* colon = pointer_cast<char*>(memmem(start, request_line_size, ": ", 2));

                    if(colon != nullptr){
                        int key_size = colon - start;
                        char* key = new char[key_size + 1];
                        strncpy(key, start, key_size);
                        key[key_size] = '\0';
                        
                        int value_size = crlf - colon - 2;
                        char* value = new char[value_size + 1];
                        strncpy(value, colon + 2, value_size);
                        value[value_size] = '\0';

                        add_header(key, value);

                        input->move_read_position_by(request_line_size + 2);
                    }
                    else{
                        //没找到
                        input->move_read_position_by(2);
                        current_state = REQUEST_DONE;
                    }
                }
            }
        }
        return ok;
    }

    char* get_url(){
        return url;
    }

    char* get_path(){
        if(path)
            return path;
        
        char* query = pointer_cast<char*>(memmem(url, strlen(url), "?", 1));
        int path_length = query ? query - url : strlen(url);

        path = new char[path_length + 1];
        strncpy(path, url, path_length);
        path[path_length] = '\0';
        return path;
    }

private:
    void clear_free_space(){
        if(version){
            delete[] version;
            version = nullptr;
        }
        if(method){
            delete[] method;
            method = nullptr;
        }
        if(url){
            delete[] url;
            url = nullptr;
        }
        if(path){
            delete[] path;
            path = nullptr;
        }
    }
    char* version;
    char* method;
    char* url;
    char* path;
    http_request_state current_state;
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

const int INIT_RESPONSE_HEADER_SIZE = 128;

class http_response{
public:
    http_response() :
        status(unknown),
        status_message(nullptr),
        content_type(nullptr),
        body(nullptr),
        response_headers(new response_header[INIT_RESPONSE_HEADER_SIZE]),
        response_headers_number(0),
        keep_connected(0)
    {}

    ~http_response(){
        delete[] response_headers;
    }

    void encode_buffer(buffer* output){
        char buf[32];
        snprintf(buf, sizeof(buf), "HTTP/1.1 %d ", status);
        output->append_string(buf);
        output->append_string(status_message);
        output->append_string("\r\n");

        if(keep_connected)
            output->append_string("Connection: close\r\n");
        else{
            snprintf(buf, sizeof(buf), "Content-Length: %zd\r\n", strlen(body));
            output->append_string(buf);
            output->append_string("Connection: Keep-alive\r\n");
        }

        if(response_headers && response_headers_number > 0){
            for(int i{}; i!=response_headers_number; ++i){
                output->append_string(response_headers[i].key);
                output->append_string(": ");
                output->append_string(response_headers[i].value);
                output->append_string("\r\n");
            }
        }

        output->append_string("\r\n");
        output->append_string(body);
    }

    virtual int request(http_request* a_http_request){
        return 0;
    }

protected:
    http_statuscode status;
    char* status_message;
    char* content_type;
    char* body;
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
        log_msg("[http connection] get message from tcp connection %s\n", name);
        if(m_http_request.parse_http_request(buf) == 0){
            char* error_response = "HTTP/1.1 400 Bad Request\r\n\r\n";
            send_data(error_response, sizeof(error_response));
            shutdown_connection();
        }

        if(m_http_request.get_current_state() == REQUEST_DONE){
            RESPONSE response;

            response.request(&m_http_request);

            buffer t_buffer;
            
            response.encode_buffer(&t_buffer);
            // log_msg("[response] encode\n%s\n", t_buffer.get_readable_data());
            t_buffer.send(this);

            if(m_http_request.close_connection())
                shutdown_connection();

            m_http_request.reset();
        }
        return 0;
    }

    ~http_connection(){
        //
        log_msg("[http connection] destructed\n");
    }
    
    int write_completed(){
        //
        return 0;
    }
private:
    http_request m_http_request;
};


#endif
