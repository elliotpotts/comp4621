#include <http/server.hpp>
#include <http/socket.hpp>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <http/request.hpp>
#include <http/response.hpp>
#include <boost/log/trivial.hpp>
#include <utility>
void errmaybe(int);

void http::server::handle_client(http::socket&& client) {
    try {
        BOOST_LOG_TRIVIAL(info) << "Thread #" << std::this_thread::get_id() << " handling new client";
        bool keep_alive = true;
        do {
            std::optional<http::request> maybe_req = http::recv_request(client);
            if(!maybe_req) {
                BOOST_LOG_TRIVIAL(error) << "Failed to read request from new connection";
            }
            http::request& req = *maybe_req;
            // Now consider
            // * Should we keep-alive the connection?
            //     Yes, unless Connection: close
            // * What is the transfer-encoding?
            //     We should support chunked
            // * What is the content-encoding?
            //     We should support gzip
            // * Which language should be sent?
            // These questions make up the "content negotiation algorithm"

            // For debug purposes:
            BOOST_LOG_TRIVIAL(info) << req.method << " " << req.uri << " " << req.version;
            for(auto pair : req.headers) {
                BOOST_LOG_TRIVIAL(info) << pair.first << ":" << pair.second;
            }
            
            http::response res = handle_request(req);
            BOOST_LOG_TRIVIAL(info) << "-------";
            http::send_response(client, res);
        } while (keep_alive);
    } catch (const http::response& err) {
        http::send_response(client, err);
        BOOST_LOG_TRIVIAL(error) << "Something went wrong: " << err.code;
    }
}

#include <fstream>
#include <sstream>
http::response http::server::handle_request(http::request req) {
    std::stringstream uri_strm;
    uri_strm << "srv" << req.uri;
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