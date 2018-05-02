#ifndef COMP4621_SERVER_HPP_INCLUDED
#define COMP4621_SERVER_HPP_INCLUDED
#include <http/worker_pool.hpp>
#include <http/session.hpp>
namespace http {
    struct socket;
    class server {
        int sockfd;
        worker_pool<session> workers;

        public:
        server(short port, int n_threads = 4);
        ~server();
        void serve_forever();
    };
}
#endif