cmake_minimum_required(VERSION 3.24)
project(market_data_fabric VERSION 0.1.0 LANGUAGES CXX)

if (POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MDF_BUILD_APPS "Build command-line applications" ON)
option(MDF_BUILD_TESTS "Build tests" ON)
option(MDF_ENABLE_NATS "Build NATS transport/client modules" OFF)
option(MDF_ENABLE_KAFKA "Build Kafka transport/client modules" OFF)
option(MDF_ENABLE_SHM "Build shared-memory transport/client modules" ON)
option(MDF_ENABLE_MULTICAST "Build UDP multicast transport modules" ON)
option(MDF_ENABLE_BEAST_WS "Build Boost.Beast WebSocket backend when headers are available" ON)

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

add_library(md-net INTERFACE)
target_include_directories(md-net INTERFACE libs/md-net/include)
if (MDF_ENABLE_BEAST_WS)
  find_package(OpenSSL QUIET)
  find_package(Boost QUIET)
  if (OpenSSL_FOUND AND Boost_FOUND)
    target_compile_definitions(md-net INTERFACE MDF_NET_ENABLE_BEAST=1)
    target_link_libraries(md-net INTERFACE OpenSSL::SSL OpenSSL::Crypto)
    if (TARGET Boost::headers)
      target_link_libraries(md-net INTERFACE Boost::headers)
    elseif (TARGET Boost::boost)
      target_link_libraries(md-net INTERFACE Boost::boost)
    endif()
  else()
    target_compile_definitions(md-net INTERFACE MDF_NET_ENABLE_BEAST=0)
  endif()
else()
  target_compile_definitions(md-net INTERFACE MDF_NET_ENABLE_BEAST=0)
endif()

add_library(md-replay INTERFACE)
target_include_directories(md-replay INTERFACE libs/md-replay/include)
target_link_libraries(md-replay INTERFACE md-core)

add_library(md-adapters-binance INTERFACE)
target_include_directories(md-adapters-binance INTERFACE libs/md-adapters/crypto/binance/include)
target_link_libraries(md-adapters-binance INTERFACE md-core md-net)

add_library(md-client-core INTERFACE)
target_include_directories(md-client-core INTERFACE libs/md-client/core/include)
target_link_libraries(md-client-core INTERFACE md-core)

if (MDF_BUILD_APPS)
  add_executable(md-node apps/md-node/src/main.cpp)
  target_link_libraries(md-node PRIVATE md-core md-refdata md-book md-runtime md-client-core md-adapters-binance)
endif()

if (MDF_BUILD_TESTS)
  enable_testing()

  add_executable(mdf-message-log-smoke tests/message_log_smoke.cpp)
  target_link_libraries(mdf-message-log-smoke PRIVATE md-core md-replay md-adapters-binance)
  add_test(NAME message_log_smoke COMMAND mdf-message-log-smoke)

  add_executable(mdf-binance-feeds-smoke tests/binance_feeds_smoke.cpp)
  target_link_libraries(mdf-binance-feeds-smoke PRIVATE md-adapters-binance)
  add_test(NAME binance_feeds_smoke COMMAND mdf-binance-feeds-smoke)

  add_executable(mdf-websocket-endpoint-smoke tests/websocket_endpoint_smoke.cpp)
  target_link_libraries(mdf-websocket-endpoint-smoke PRIVATE md-net)
  add_test(NAME websocket_endpoint_smoke COMMAND mdf-websocket-endpoint-smoke)

  add_executable(mdf-spsc-ring-smoke tests/spsc_ring_smoke.cpp)
  target_link_libraries(mdf-spsc-ring-smoke PRIVATE md-core md-runtime)
  add_test(NAME spsc_ring_smoke COMMAND mdf-spsc-ring-smoke)
endif()
