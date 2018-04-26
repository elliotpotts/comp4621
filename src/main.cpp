#include <iostream>
#include <http/server.hpp>
#include <signal.h>

void errmaybe(int return_val) {
    if(return_val < 0 && errno > 0) {
        throw std::system_error(errno, std::generic_category());
    }
}

void handle_signal(int) {
    // Do nothing, we'll just handle the exception thrown
    // when the next "system call" returns EINTR
}

int main() {
    // Install signal handler for SIGINT aka keyboard interrupt
    struct sigaction sa;
    sa.sa_handler = handle_signal;
    //sa.sa_mask = 0;
    sa.sa_flags = 0;
    sa.sa_restorer = nullptr;
    errmaybe(sigaction(SIGINT, &sa, nullptr));
    
    // Start server
    std::cout << "Listening...\n";
    try {
        auto s = http::server{9999};
        s.serve_forever();
    } catch (const std::system_error& ex) {
        if (ex.code().value() == EINTR) {
            std::cout << "Keyboard interrupt. Stopping server" << std::endl;
        } else {
            throw;
        }
    }
}