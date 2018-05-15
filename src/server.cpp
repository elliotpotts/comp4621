#include <http/server.hpp>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <boost/log/trivial.hpp>
void errmaybe(int);

http::server::server(short port, int n_threads) : sockfd(::socket(AF_INET, SOCK_STREAM, 0)), workers(n_threads) {
    errmaybe(sockfd);
    int enable_reuse = 1;
    errmaybe(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable_reuse, sizeof(enable_reuse)));
    const sockaddr_in listen_address = {
        AF_INET,      // sin_family
        htons(port),  // sin_port
        INADDR_ANY    // sin_addr
    };
    errmaybe(::bind(sockfd, reinterpret_cast<const sockaddr*>(&listen_address), sizeof(listen_address)));
    errmaybe(::listen(sockfd, n_threads));
}

http::server::~server() {
    ::close(sockfd);
}

static void set_timeout(int fd) {
    timeval timeout;
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;
    errmaybe(::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)));
    errmaybe(::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)));
}

void http::server::serve_forever() {
    while(true) {
        sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);
        int clientfd = ::accept(sockfd, reinterpret_cast<sockaddr*>(&client_addr), &addrlen);
        set_timeout(clientfd);
        BOOST_LOG_TRIVIAL(info) << "        accepted fd #" << clientfd;
        errmaybe(clientfd);
        workers.post_task(clientfd);
    }
}