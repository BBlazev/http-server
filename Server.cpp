#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <thread>
#include <vector>
#include "ThreadPool.hpp"



const size_t MAX_HEADERS = 8192;


struct Connection
{
    int fd;
    std::vector<char> buffer; //accumulated req bytes
    bool headers_done = false; // \r\n\r\n yet?
};

ssize_t write_all(int fd, const char* buf, size_t count)
{
    size_t written = 0;
    while (written < count) {
        ssize_t n = write(fd, buf + written, count - written);
        if (n <= 0) return n;
        written += n;
    }
    return written;
}

const char* mime_type(const std::string& path)
{
    auto dot = path.find_last_of('.');
    if (dot == std::string::npos) return "application/octet-stream";
    std::string ext = path.substr(dot + 1);
    if (ext == "html" || ext == "htm") return "text/html";
    if (ext == "css")  return "text/css";
    if (ext == "js")   return "application/javascript";
    if (ext == "json") return "application/json";
    if (ext == "png")  return "image/png";
    if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
    if (ext == "gif")  return "image/gif";
    if (ext == "svg")  return "image/svg+xml";
    if (ext == "txt")  return "text/plain";
    return "application/octet-stream";
}


void handle_client(Connection* conn)
{
    // extract method
    char* method_end = (char*)memchr(conn->buffer.data(), ' ', conn->buffer.size());
    if (!method_end) {
        //epoll_ctl(epfd, EPOLL_CTL_DEL, conn->fd, nullptr);
        close(conn->fd);
        delete conn;
        return;
    }
    size_t method_len = method_end - conn->buffer.data();
    if (!(method_len == 3 && memcmp(conn->buffer.data(), "GET", 3) == 0)) {
        const char* resp = "HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 18\r\n\r\nMethod Not Allowed";
        write_all(conn->fd, resp, strlen(resp));
        //epoll_ctl(epfd, EPOLL_CTL_DEL, conn->fd, nullptr);
        close(conn->fd);
        delete conn;
        return;
    }

    // extract path
    char* path_start = method_end + 1;
    size_t remaining = conn->buffer.size() - (path_start - conn->buffer.data());
    char* path_end = (char*)memchr(path_start, ' ', remaining);
    if (!path_end) {
        //epoll_ctl(epfd, EPOLL_CTL_DEL, conn->fd, nullptr);
        close(conn->fd);
        delete conn;
        return;
    }
    std::string path(path_start, path_end);

    // path traversal check
    if (path.find("..") != std::string::npos) {
        const char* resp = "HTTP/1.1 403 Forbidden\r\nContent-Length: 9\r\n\r\nForbidden";
        write_all(conn->fd, resp, strlen(resp));
        //epoll_ctl(epfd, EPOLL_CTL_DEL, conn->fd, nullptr);
        close(conn->fd);
        delete conn;
        return;
    }

    std::string local_path = (path == "/") ? "./index.html" : "." + path;

    std::ifstream file(local_path, std::ios::binary);
    if (!file.is_open()) {
        printf("File not found: %s\n", local_path.c_str());
        fflush(stdout);
        const char* not_found = "HTTP/1.1 404 Not Found\r\nContent-Length: 13\r\n\r\n404 Not Found";
        write_all(conn->fd, not_found, strlen(not_found));
        
        //epoll_ctl(epfd, EPOLL_CTL_DEL, conn->fd, nullptr);
        close(conn->fd);
        delete conn;

        return;
    }

    file.seekg(0, std::ios::end);
    std::streamsize file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::ostringstream hdr;
    hdr << "HTTP/1.1 200 OK\r\n"
        << "Content-Type: " << mime_type(local_path) << "\r\n"
        << "Content-Length: " << file_size << "\r\n"
        << "\r\n";
    std::string headers = hdr.str();
    write_all(conn->fd, headers.data(), headers.size());

    char file_buffer[8192];
    while (file) {
        file.read(file_buffer, sizeof(file_buffer));
        std::streamsize got = file.gcount();
        if (got > 0) write_all(conn->fd, file_buffer, got);
    }

    printf("Successfully served: %s\n", local_path.c_str());
    fflush(stdout);
    //epoll_ctl(epfd, EPOLL_CTL_DEL, conn->fd, nullptr);
    close(conn->fd);
    delete conn;
    return;
}


int main() {
    //std::thread::hardware_concurrency();
    ThreadPool pool(10);
    int tcp = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);

    if (tcp < 0) {
        perror("Error opening socket");
        return 1;
    }
    int opt = 1;

    if (setsockopt(tcp, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        perror("setsockopt failed");

    struct sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(8080);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(tcp, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind failed");
        return 1;
    }

    if (listen(tcp, 10) < 0) {
        perror("listen failed");
        return 1;
    }
    printf("Server listening on port 8080\n");
    fflush(stdout);
    
    //epoll instance
    int epfd = epoll_create1(0);
    struct epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = tcp;
    ev.data.ptr = nullptr;
    epoll_ctl(epfd, EPOLL_CTL_ADD, tcp, &ev);

    struct epoll_event events[64];

    while (true) {
        int n = epoll_wait(epfd, events, 64, -1); //-1 wait forever
        for(int i = 0; i < n; i++)
        {
            //int fd = events[i].data.fd;
            if (events[i].data.ptr == nullptr)
            {
                while(true)
                {
                    int client_fd = accept4(tcp, nullptr, nullptr, SOCK_NONBLOCK);
                    if(client_fd < 0)
                    {
                        if(errno == EAGAIN || errno == EWOULDBLOCK) break;
                        perror("accept");
                        break;
                    }
                    Connection* conn = new Connection{client_fd};
                    struct epoll_event cev{};
                    cev.events = EPOLLIN;
                    cev.data.ptr = conn;
                    epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &cev);
                }
                
            }
            else
            {
                Connection* conn = (Connection*)events[i].data.ptr;

                char temp[4096];
                bool done = false;
                bool should_close = false;

                while (true) 
                {
                    ssize_t n = read(conn->fd, temp, sizeof(temp));
                    
                    if (n > 0) 
                        conn->buffer.insert(conn->buffer.end(), temp, temp + n);
                        // continue, drain more
                    else if (n == 0) 
                    {
                        should_close = true;  // peer closed
                        break;
                    }
                    else 
                    {  
                        if (errno == EAGAIN || errno == EWOULDBLOCK) break;  // drained
                        if (errno == EINTR) continue;  // signal interrupted, retry
                        should_close = true;  // real error
                        break;
                    }
                }

                if (should_close) 
                {
                    epoll_ctl(epfd, EPOLL_CTL_DEL, conn->fd, nullptr);
                    close(conn->fd);
                    delete conn;
                    continue;  
                }

                if (conn->buffer.size() > MAX_HEADERS) 
                {
                    epoll_ctl(epfd, EPOLL_CTL_DEL, conn->fd, nullptr);
                    close(conn->fd);
                    delete conn;
                    continue;
                }

                //dispatch path?
                if (memmem(conn->buffer.data(), conn->buffer.size(), "\r\n\r\n", 4))
                {
                    epoll_ctl(epfd, EPOLL_CTL_DEL, conn->fd, nullptr);
                    pool.submit([conn]()
                        {
                            handle_client(conn);
                        }
                    );

                } 
                

            }
        }
    }        

    return 0;
}