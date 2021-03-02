#include"mrs.hpp"

class a_http_response : public http_response{
public:
    int request(http_request* a_http_request)override {
        char* url = a_http_request->get_url();
        char* path = a_http_request->get_path();

        if(strcmp(path, "/") == 0){
            status = ok;
            status_message = "OK";
            content_type = "text/html";
            body = 
            "<html>"
                "<head>"
                    "<title>"
                        "r0s,http"
                    "</title>"
                "</head>"
                "<body>"
                    "<h1>"
                        "datong"
                    "</h1>"
                "</body>"
            "</html>";
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
            content_type = "text/html";
            body = 
            "<html><head>"
            "<title>404 Not Found</title>"
            "</head><body>"
            "<h1>Not Found</h1>"
            "<p>The requested URL was not found on this server.</p>"
            "</body></html>";

            // keep_connected = 1;
        }
        return 0;
    }

private:
};

int main(){

    TCPserver<http_connection<a_http_response>> tcp_server(80, 2);
    
    tcp_server.start();

    tcp_server.run();

    return 0;
}