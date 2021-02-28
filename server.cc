#include"mrs.hpp"
class a_http_response : public http_response{
public:
    int request(http_request* a_http_request)override {
        char* url = a_http_request->get_url();
        char* question = pointer_cast<char*>(memmem(url, strlen(url), "?", 1));
        char* path{};

        if(question){
            path = new char[question - url];
            strncpy(path, url, question - url);
        }
        else{
            path = new char[strlen(url)];
            strncpy(path, url, strlen(url));
        }

        if(strcmp(path, "/") == 0){
            status = ok;
            status_message = "OK";
            content_type = "text/html";
            body = "<html><head><title>r0s,http</title></head><body><h1>datong</h1></body></html>";
        }
        else if(strcmp(path, "/network") == 0){
            status = ok;
            status_message = "OK";
            content_type = "text/plain";
            body = "r0test";
        }
        else{
            status = not_found;
            status_message = "Not Found";
            keep_connected = 1;
        }
        return 0;
    }

private:
};

int main(){
    event_loop a_event_loop;

    acceptor a_acceptor(80);

    TCPserver<http_connection<a_http_response>> tcp_server(&a_event_loop, &a_acceptor, 2);
    
    tcp_server.start();

    a_event_loop.run();

    return 0;
}