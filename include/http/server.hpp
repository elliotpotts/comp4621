#ifndef COMP4621_SERVER_HPP_INCLUDED
#define COMP4621_SERVER_HPP_INCLUDED
#include <http/threadpool.hpp>
#include <http/response.hpp>
#include <http/request.hpp>
namespace http {
    struct socket;
    class server {
        static const int backlog = 128; // arbitrary
        int fd;
        threadpool workers;
        void handle_client(socket&& client);

        protected:
        virtual response handle_request(request);
        
        public:
        server(short port);
        virtual ~server();
        void serve_forever();
    };
}
#endif