#ifndef COMMON_HPP
#define COMMON_HPP

#include <boost/log/trivial.hpp>

#define LOG(severity) BOOST_LOG_TRIVIAL(severity)
#define dprint(var) LOG(info) << #var << " = " << var

#ifndef NDEBUG
#define DEBUG
#endif

void fail_if(const bool condition, const std::string &message){
    if(condition){
        LOG(error) << message;
        exit(1);
    }
}

#endif
