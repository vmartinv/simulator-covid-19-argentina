cmake_minimum_required(VERSION 3.14)

set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
add_definitions(-DBOOST_LOG_DYN_LINK)
find_package(Boost 1.67.0 COMPONENTS log_setup log date_time system thread regex chrono filesystem program_options)
find_package(nlohmann_json 3.2.0 REQUIRED)

include_directories(${Boost_INCLUDE_DIRS})

file(GLOB simulation_SRC
    "*.hpp"
    "*.cpp"
)
add_executable(simulation ${simulation_SRC})
target_compile_features(simulation PRIVATE cxx_std_17)
target_link_libraries(simulation PRIVATE nlohmann_json::nlohmann_json ${Boost_LIBRARIES})
