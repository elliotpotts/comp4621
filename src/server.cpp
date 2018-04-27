#include <http/server.hpp>
#include <http/socket.hpp>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <http/request.hpp>
#include <http/response.hpp>
#include <iostream>
void errmaybe(int);

void http::server::handle_client(http::socket&& client) {
    try {
        std::cout << "Thread #" << std::this_thread::get_id() << " handling new client\n";
        auto req = http::recv_request(client);
        http::response res = handle_request(req);
        http::send_response(client, res);
    } catch (const http::response& err) {
        http::send_response(client, err);
        std::cout << "Something went wrong: " << err.code << std::endl;
    }
}

#include <fstream>
#include <sstream>
http::response http::server::handle_request(http::request req) {
    std::stringstream uri_strm;
    uri_strm << "srv" << req.uri;
    std::cout << uri_strm.str() << std::endl;
    std::fstream input{uri_strm.str()};
    if(input.is_open()) {
        return {200, {
            std::istreambuf_iterator<char>{input},
            std::istreambuf_iterator<char>{}
        }};
    } else {
        return {404, "File not found :("};
    }
}

http::server::server(short port) : fd(::socket(AF_INET, SOCK_STREAM, 0)), workers(backlog) {
    errmaybe(fd);
    int enable_reuse = 1;
    errmaybe(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable_reuse, sizeof(enable_reuse)));
    const sockaddr_in listen_address = {
        AF_INET,      // sin_family
        htons(port),  // sin_port
        INADDR_ANY    // sin_addr
    };
    errmaybe(::bind(fd, reinterpret_cast<const sockaddr*>(&listen_address), sizeof(listen_address)));
    errmaybe(::listen(fd, backlog));
}

http::server::~server() {
    ::close(fd);
}

void http::server::serve_forever() {
    while(true) {
        sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);
        int clientfd = ::accept(fd, reinterpret_cast<sockaddr*>(&client_addr), &addrlen);
        errmaybe(clientfd);
        workers.post_task([clientfd,this](){
            handle_client({clientfd});
        });
    }
}