#include"http_server.h"

//http_request

static const int INIT_REQUEST_HEADER_SIZE = 128;
static const char* HTTP10 = "HTTP/1.0";
static const char* HTTP11 = "HTTP/1.1";
static const char* KEEP_ALIVE = "Keep-Alive";
static const char* CLOSE = "close";

http_request::http_request() :
    version(nullptr),
    method(nullptr),
    url(nullptr),
    path(nullptr),
    current_state(0),
    request_headers(new request_header[INIT_REQUEST_HEADER_SIZE]),
    request_headers_number(0)
{}

http_request::~http_request(){
    clear_free_space();

    if(request_headers){
        delete[] request_headers;
    }
}

void http_request::reset(){
    clear_free_space();
    current_state = 0;
    request_headers_number = 0;
}

const char* http_request::get_header(const char* key){
    if(request_headers){
        for(int i{}; i != request_headers_number; ++i)
            if(strncmp(request_headers[i].key, key, strlen(key)) == 0)
                return request_headers[i].value;
    }
    return nullptr;
}

int http_request::is_parsed(){
    return current_state;
}

int http_request::close_connection(){
    const char* connection = get_header("Connection");

    if(connection && strncmp(connection, CLOSE, strlen(CLOSE)) == 0)
        return 1;

    if(version && strncmp(version, HTTP10, strlen(HTTP10)) == 0 &&
        strncmp(connection, KEEP_ALIVE, strlen(KEEP_ALIVE)) == 1){
            return 1;
        }
    return 0;
}

int http_request::parse_http_request(buffer* input){

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

char* http_request::get_url(){
    return url;
}

char* http_request::get_path(){
    if(path)
        return path;
    
    char* query = pointer_cast<char*>(memmem(url, strlen(url), "?", 1));
    int path_length = query ? query - url : strlen(url);

    path = new char[path_length + 1];
    strncpy(path, url, path_length);
    path[path_length] = '\0';

    return path;
}
//private
void http_request::clear_free_space(){
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


//http-response

static const int INIT_RESPONSE_HEADER_SIZE = 128;

http_response::http_response() :
    status(unknown),
    status_message(nullptr),
    content_type(nullptr),
    body{},
    response_headers(new response_header[INIT_RESPONSE_HEADER_SIZE]),
    response_headers_number(0),
    keep_connected(0)
{}

http_response::~http_response(){
    delete[] response_headers;
}

void http_response::encode_buffer(buffer* output){
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

int http_response::request(http_request* a_http_request){
    return 0;
}

