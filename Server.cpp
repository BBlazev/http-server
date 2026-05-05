#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <thread>

#include "ThreadPool.hpp"

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

int main() {
    ThreadPool pool(10);
    int tcp = socket(AF_INET, SOCK_STREAM, 0);

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

    while (true) {
        int new_fd = accept(tcp, nullptr, nullptr);
        if (new_fd < 0) 
        {
            perror("accept failed");
            continue;
        }
        printf("Client connected\n");
        fflush(stdout);

        pool.submit([new_fd]() {
            char buffer[8192];
            size_t total = 0;

            // read until \r\n\r\n
            while (true) {
                ssize_t n = read(new_fd, buffer + total, sizeof(buffer) - total);
                if (n <= 0) {
                    close(new_fd);
                    return;
                }
                total += n;

                if (memmem(buffer, total, "\r\n\r\n", 4)) break;

                // headers too big
                if (total >= sizeof(buffer)) {
                    close(new_fd);
                    return;
                }
            }

            // extract method
            char* method_end = (char*)memchr(buffer, ' ', total);
            if (!method_end) {
                close(new_fd);
                return;
            }
            size_t method_len = method_end - buffer;
            if (!(method_len == 3 && memcmp(buffer, "GET", 3) == 0)) {
                const char* resp = "HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 18\r\n\r\nMethod Not Allowed";
                write_all(new_fd, resp, strlen(resp));
                close(new_fd);
                return;
            }

            // extract path
            char* path_start = method_end + 1;
            size_t remaining = total - (path_start - buffer);
            char* path_end = (char*)memchr(path_start, ' ', remaining);
            if (!path_end) {
                close(new_fd);
                return;
            }
            std::string path(path_start, path_end);

            // path traversal check
            if (path.find("..") != std::string::npos) {
                const char* resp = "HTTP/1.1 403 Forbidden\r\nContent-Length: 9\r\n\r\nForbidden";
                write_all(new_fd, resp, strlen(resp));
                close(new_fd);
                return;
            }

            std::string local_path = (path == "/") ? "./index.html" : "." + path;

            std::ifstream file(local_path, std::ios::binary);
            if (!file.is_open()) {
                printf("File not found: %s\n", local_path.c_str());
                fflush(stdout);
                const char* not_found = "HTTP/1.1 404 Not Found\r\nContent-Length: 13\r\n\r\n404 Not Found";
                write_all(new_fd, not_found, strlen(not_found));
                close(new_fd);
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
            write_all(new_fd, headers.data(), headers.size());

            char file_buffer[8192];
            while (file) {
                file.read(file_buffer, sizeof(file_buffer));
                std::streamsize got = file.gcount();
                if (got > 0) write_all(new_fd, file_buffer, got);
            }

            printf("Successfully served: %s\n", local_path.c_str());
            fflush(stdout);

            file.close();
            close(new_fd);
        });
        

        // std::thread t1([new_fd]() {
        //     char buffer[8192];
        //     size_t total = 0;

        //     // read until \r\n\r\n
        //     while (true) {
        //         ssize_t n = read(new_fd, buffer + total, sizeof(buffer) - total);
        //         if (n <= 0) {
        //             close(new_fd);
        //             return;
        //         }
        //         total += n;

        //         if (memmem(buffer, total, "\r\n\r\n", 4)) break;

        //         // headers too big
        //         if (total >= sizeof(buffer)) {
        //             close(new_fd);
        //             return;
        //         }
        //     }

        //     // extract method
        //     char* method_end = (char*)memchr(buffer, ' ', total);
        //     if (!method_end) {
        //         close(new_fd);
        //         return;
        //     }
        //     size_t method_len = method_end - buffer;
        //     if (!(method_len == 3 && memcmp(buffer, "GET", 3) == 0)) {
        //         const char* resp = "HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 18\r\n\r\nMethod Not Allowed";
        //         write_all(new_fd, resp, strlen(resp));
        //         close(new_fd);
        //         return;
        //     }

        //     // extract path
        //     char* path_start = method_end + 1;
        //     size_t remaining = total - (path_start - buffer);
        //     char* path_end = (char*)memchr(path_start, ' ', remaining);
        //     if (!path_end) {
        //         close(new_fd);
        //         return;
        //     }
        //     std::string path(path_start, path_end);

        //     // path traversal check
        //     if (path.find("..") != std::string::npos) {
        //         const char* resp = "HTTP/1.1 403 Forbidden\r\nContent-Length: 9\r\n\r\nForbidden";
        //         write_all(new_fd, resp, strlen(resp));
        //         close(new_fd);
        //         return;
        //     }

        //     std::string local_path = (path == "/") ? "./index.html" : "." + path;

        //     std::ifstream file(local_path, std::ios::binary);
        //     if (!file.is_open()) {
        //         printf("File not found: %s\n", local_path.c_str());
        //         fflush(stdout);
        //         const char* not_found = "HTTP/1.1 404 Not Found\r\nContent-Length: 13\r\n\r\n404 Not Found";
        //         write_all(new_fd, not_found, strlen(not_found));
        //         close(new_fd);
        //         return;
        //     }

        //     file.seekg(0, std::ios::end);
        //     std::streamsize file_size = file.tellg();
        //     file.seekg(0, std::ios::beg);

        //     std::ostringstream hdr;
        //     hdr << "HTTP/1.1 200 OK\r\n"
        //         << "Content-Type: " << mime_type(local_path) << "\r\n"
        //         << "Content-Length: " << file_size << "\r\n"
        //         << "\r\n";
        //     std::string headers = hdr.str();
        //     write_all(new_fd, headers.data(), headers.size());

        //     char file_buffer[8192];
        //     while (file) {
        //         file.read(file_buffer, sizeof(file_buffer));
        //         std::streamsize got = file.gcount();
        //         if (got > 0) write_all(new_fd, file_buffer, got);
        //     }

        //     printf("Successfully served: %s\n", local_path.c_str());
        //     fflush(stdout);

        //     file.close();
        //     close(new_fd);
        // });
        // t1.detach();
    }

    return 0;
}