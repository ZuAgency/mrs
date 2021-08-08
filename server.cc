#include"mrs.h"
#include"mrs/content_type.h"

class my_tcp_connection : public tcp_connection{
public:
    using tcp_connection::tcp_connection;

    ~my_tcp_connection(){
        log_msg("connection closed\n");
    }

    int message(buffer* input) override{
        log_msg("get %d bytes from %s : %s\n", input->get_readable_size(), name, input->get_readable_data());

        buffer output;
        int size = input->get_readable_size();
        for(int i{}; i!= size; ++i){
            char c = input->read_char();
            if(c >= 'a' && c <= 'z')
                c-=32;
            output.append_char(c);
        }
        output.send(this);
        return 0;
    }

    int write_completed() override{
        log_msg("write completed\n");
        return 0;
    }
};

class a_http_response : public http_response{
public:
    a_http_response() {}

    ~a_http_response(){
        
    }
    int request(http_request* a_http_request)override {
        char* url = a_http_request->get_url();
        const char* path = a_http_request->get_path();

        const char* root = "/root/Test/public";
        const char* index = "index.html";

        char u[1024];
        strcpy(u, root);
        strcat(u, path);

        log_msg("[request path] %s\n", path);
        
        int f = is_file(u);
        if(!f){
            if(path[strlen(path) - 1] != '/')
                strcat(u, "/");
            strcat(u, index);
            f = is_file(u);
        }
       if(f){        
            int fd = open(u, O_RDONLY);

            status = ok;
            status_message = "OK";
            
            const char* ext = get_extension(u);
            content_type = get_content_type(ext);
            
            char s[10001];
            int nread;
            int sumread = 0;
            body.clear();
            while((nread = read(fd, s, 10000)) > 0){
                sumread += nread;
                s[nread] = 0;
                body.append(s, nread);

            }
        }
        else{
            status = not_found;
            status_message = "Not Found";
            content_type = "text/html";
            body.append_string(
            "<html><head>"
            "<title>404 Not Found</title>"
            "</head><body>"
            "<h1>Not Found</h1>"
            "<p>The requested URL was not found on this server.</p>"
            "</body></html>");

            // keep_connected = 1;
        }
        return 0;
    }

private:
    bool is_file(const char* path){
        struct stat st{};
        int r = stat(path, &st);

        if(r)
           return 0;
        
        return st.st_mode & S_IFREG;
    
    }
};

int main(){

    //TCPserver<my_tcp_connection> tcp_server(6000, 2);
    TCPserver<http_connection<a_http_response>> tcp_server(80, 2);
    
    tcp_server.start();

    tcp_server.run();

    return 0;
}

/*
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
*/
