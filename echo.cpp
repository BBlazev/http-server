#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <unistd.h>
#include <thread>

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


int main() {                          
    int tcp = socket(AF_INET, SOCK_STREAM, 0);

    if(tcp < 0) 
    {
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

    if (listen(tcp, 10) < 0)
    {
        perror("listen failed");
        return 1;
    }
    printf( "Server listening on port 8080\n");
    fflush(stdout);

    //char buffer[1024];

//int accept(int sockfd, struct sockaddr *_Nullable restrict addr,
	//socklen_t *_Nullable restrict addrlen);
    
    //socklen_t addrlen = sizeof(addr);
    while(true)
    {
        int new_fd = accept(tcp, nullptr, nullptr);
        if(new_fd < 0) {
            perror("accept failed");
            continue;
        }
        printf("Client connected\n");
        fflush(stdout);


        std::thread t1([new_fd](){
            char buffer[1024];
            while(true){
                ssize_t n = read(new_fd, buffer, 1024);
                if (n <= 0) return;
                write_all(new_fd, buffer, n);
            }
            close(new_fd);
            printf("Client disconnected\n");
        });
        t1.detach();



        fflush(stdout);

    }

    return 0;
}