#ifndef COMP4621_ERROR_HPP_INCLUDED
#define COMP4621_ERROR_HPP_INCLUDED
#include <stdexcept>
namespace http {
    void check_error(int return_val);
    struct premature_close : public std::runtime_error {
        using std::runtime_error::runtime_error;
    };
}
#endif