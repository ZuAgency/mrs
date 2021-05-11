#ifndef HTTP_SERVER_HPP
#define HTTP_SERVER_HPP

#include"common.hpp"
#include"tcp_server.hpp"
#include"picohttpparser.h"

//http-request
const int INIT_REQUEST_HEADER_SIZE = 128;
const char* HTTP10 = "HTTP/1.0";
const char* HTTP11 = "HTTP/1.1";
const char* KEEP_ALIVE = "Keep-Alive";
const char* CLOSE = "close";

class http_request{
public:
    http_request() :
        version(nullptr),
        method(nullptr),
        url(nullptr),
        path(nullptr),
        current_state(0),
        request_headers(new request_header[INIT_REQUEST_HEADER_SIZE]),
        request_headers_number(0)
    {}

    ~http_request(){
        clear_free_space();

        if(request_headers){
            delete[] request_headers;
        }
    }

    void reset(){
        clear_free_space();
        current_state = 0;
        request_headers_number = 0;
    }

    const char* get_header(const char* key){
        if(request_headers){
            for(int i{}; i != request_headers_number; ++i)
                if(strncmp(request_headers[i].key, key, strlen(key)) == 0)
                    return request_headers[i].value;
        }
        return nullptr;
    }

    int is_parsed(){
        return current_state;
    }

    int close_connection(){
        const char* connection = get_header("Connection");

        if(connection && strncmp(connection, CLOSE, strlen(CLOSE)) == 0)
            return 1;

        if(version && strncmp(version, HTTP10, strlen(HTTP10)) == 0 &&
            strncmp(connection, KEEP_ALIVE, strlen(KEEP_ALIVE)) == 1){
                return 1;
            }
        return 0;
    }

    int parse_http_request(buffer* input){
 
        const char *parsed_method, *parsed_path;
        int pret, minor_version;
        struct phr_header headers[INIT_REQUEST_HEADER_SIZE];
        size_t buflen = input->get_readable_size() + 1;
        size_t pbuflen = 0, method_len, path_len, num_headers = INIT_REQUEST_HEADER_SIZE;

        //printf("prase----VVVV\n");
        //printf("%s<<\n", input->get_readable_data());
        pret = phr_parse_request(input->get_readable_data(), buflen, &parsed_method, &method_len, &parsed_path, 
                                &path_len, &minor_version, headers, &num_headers, pbuflen);

        if(pret <= 0){
            if(pret == -1){
                //printf("parser error\n");
                return 0;
            }
            assert(pret == -2);
            if(buflen == sizeof(buffer)){
                //printf("request is too long error\n");
                return -1;
            }
        }
        // printf("method: %.*s\n", (int)method_len, parsed_method);
        // printf("path: %.*s\n", (int)path_len, parsed_path);
        // printf("HTTP version: 1.%d\n", minor_version);
        // printf("headers: %d\n", num_headers);

        current_state = 1;

        clear_free_space();
        
        //version
        version = new char[9];
        strncpy(version, "HTTP/1.1", 9);
        version[7] = minor_version + '0';

        //method
        method = new char[method_len + 1];
        strncpy(method, parsed_method, method_len);
        method[method_len] = '\0';

        //url
        url = new char[path_len + 1];
        strncpy(url, parsed_path, path_len);
        url[path_len] = '\0';

        //headers
        request_headers_number = num_headers;
        for(int i = 0; i != num_headers; ++i){
            //key
            auto key_size = headers[i].name_len;
            char* key = new char[ key_size + 1];
            strncpy(key, headers[i].name, key_size);
            key[key_size] = '\0';
            request_headers[i].key = key;

            //value
            auto value_size = headers[i].value_len;
            char* value = new char[value_size + 1];
            strncpy(value, headers[i].value, value_size);
            value[value_size] = '\0';
            request_headers[i].value = value;
        }
        
        return 1;
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
        
        for(int i = 0; i < request_headers_number; ++i){
            delete[] request_headers[i].key;
            delete[] request_headers[i].value;
        }
    }
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

const int INIT_RESPONSE_HEADER_SIZE = 128;

class http_response{
public:
    http_response() :
        status(unknown),
        status_message(nullptr),
        content_type(nullptr),
        body{},
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
        //printf("[encoding] %s", output->get_readable_data());
        if(keep_connected)
            output->append_string("Connection: close\r\n");
        else{
            snprintf(buf, sizeof(buf), "Content-Length: %zd\r\n", body.get_readable_size());
            output->append_string(buf);
            
            output->append_string("Content-Type: ");
            output->append_string(content_type);
            output->append_string("\r\n");

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
        output->append(body.get_readable_data(), body.get_readable_size());
        
   }

    virtual int request(http_request* a_http_request){
        return 0;
    }

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
