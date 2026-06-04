cmake_minimum_required(VERSION 3.24)
project(market_data_fabric VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MDF_BUILD_APPS "Build command-line applications" ON)
option(MDF_BUILD_TESTS "Build tests" ON)
option(MDF_ENABLE_NATS "Build NATS transport/client modules" OFF)
option(MDF_ENABLE_KAFKA "Build Kafka transport/client modules" OFF)
option(MDF_ENABLE_SHM "Build shared-memory transport/client modules" ON)
option(MDF_ENABLE_MULTICAST "Build UDP multicast transport modules" ON)

add_library(md-core INTERFACE)
target_include_directories(md-core INTERFACE libs/md-core/include)

add_library(md-refdata INTERFACE)
target_include_directories(md-refdata INTERFACE libs/md-refdata/include)
target_link_libraries(md-refdata INTERFACE md-core)

add_library(md-book INTERFACE)
target_include_directories(md-book INTERFACE libs/md-book/include)
target_link_libraries(md-book INTERFACE md-core)

add_library(md-runtime INTERFACE)
target_include_directories(md-runtime INTERFACE libs/md-runtime/include)

add_library(md-client-core INTERFACE)
target_include_directories(md-client-core INTERFACE libs/md-client/core/include)
target_link_libraries(md-client-core INTERFACE md-core)

if (MDF_BUILD_APPS)
  add_executable(md-node apps/md-node/src/main.cpp)
  target_link_libraries(md-node PRIVATE md-core md-refdata md-book md-runtime md-client-core)
endif()
