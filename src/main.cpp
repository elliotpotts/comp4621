#include <http/server.hpp>
#include <http/error.hpp>
#include <boost/log/trivial.hpp>
#include <signal.h>
#include <csignal>

void http::check_error(int return_val) {
    if(return_val < 0 && errno > 0) {
        throw std::system_error(errno, std::generic_category());
    }
}

int main() {
    // Ignore "broken pipe" signals (ie unexpected socket closures)
    // They are handled correctly in networking code.
    signal(SIGPIPE, SIG_IGN);
    
    // Start server
    BOOST_LOG_TRIVIAL(info) << "Listening...";
    try {
        auto s = http::server{9999, 20};
        s.serve_forever();
    } catch (const std::system_error& ex) {
        if (ex.code().value() == EINTR) {
            BOOST_LOG_TRIVIAL(info) << "Keyboard interrupt. Stopping server";
        } else {
            throw;
        }
    }
}