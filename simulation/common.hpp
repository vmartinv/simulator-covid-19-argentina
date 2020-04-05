#ifndef COMMON_HPP
#define COMMON_HPP

#include <boost/log/trivial.hpp>

#define LOG(severity) BOOST_LOG_TRIVIAL(severity)
#define dprint(var) LOG(info) << #var << " = " << var
#endif

#ifndef NDEBUG
#define DEBUG
#endif
