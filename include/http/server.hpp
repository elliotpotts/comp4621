#ifndef COMP4621_SERVER_HPP_INCLUDED
#define COMP4621_SERVER_HPP_INCLUDED
namespace http {
    struct socket;
    class server {
        static const int backlog = 128; // arbitrary
        int fd;
        void handle_client(socket&& client);
        
        public:
        server(short port);
        ~server();
        void serve_forever();
    };
}
#endif