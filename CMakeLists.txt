cmake_minimum_required(VERSION 3.24)
project(marketdata VERSION 0.1.0 LANGUAGES CXX)

if (POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MDF_BUILD_APPS "Build command-line applications" ON)
option(MDF_BUILD_TESTS "Build tests" ON)
option(MDF_BUILD_REPLAY_TESTS "Build replay/message-log tests" OFF)
option(MDF_ENABLE_NATS "Build NATS transport/client modules" OFF)
option(MDF_ENABLE_KAFKA "Build Kafka transport/client modules" OFF)
option(MDF_ENABLE_SHM "Build shared-memory transport/client modules" ON)
option(MDF_ENABLE_MULTICAST "Build UDP multicast transport modules" ON)
option(MDF_ENABLE_BEAST_WS "Build Boost.Beast WebSocket backend when headers are available" ON)
set(MDF_TRADING_CORE_INCLUDE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../trading-core/include"
    CACHE PATH "Path to trading-core/include for adapters that emit shared trading events")
set(MDF_ORDERBOOK_INCLUDE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../orderbook/include"
    CACHE PATH "Path to orderbook/include for offline book check tools")

include(cmake/Dependencies.cmake)

if (NOT TARGET trading_core AND
    EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../trading-core/CMakeLists.txt")
  set(TRADING_CORE_BUILD_TESTS OFF CACHE BOOL
      "Build trading-core tests" FORCE)
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../trading-core"
                   "${CMAKE_BINARY_DIR}/_deps/trading-core")
endif()

add_library(marketdata INTERFACE)
target_include_directories(marketdata INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_compile_features(marketdata INTERFACE cxx_std_20)
if (TARGET trading_core)
  target_link_libraries(marketdata INTERFACE trading_core)
endif()

mdf_configure_quill(MDF_QUILL_FOUND MDF_QUILL_TARGET)
if (MDF_QUILL_FOUND)
  target_compile_definitions(marketdata INTERFACE MDF_RUNTIME_ENABLE_QUILL=1)
  target_link_libraries(marketdata INTERFACE ${MDF_QUILL_TARGET})
else()
  target_compile_definitions(marketdata INTERFACE MDF_RUNTIME_ENABLE_QUILL=0)
endif()

if (MDF_ENABLE_BEAST_WS)
  find_package(OpenSSL QUIET)
  mdf_configure_boost_headers(MDF_BOOST_HEADERS_FOUND MDF_BOOST_HEADERS_TARGET)
  if (OpenSSL_FOUND AND MDF_BOOST_HEADERS_FOUND)
    target_compile_definitions(marketdata INTERFACE MDF_NET_ENABLE_BEAST=1)
    target_link_libraries(marketdata INTERFACE OpenSSL::SSL OpenSSL::Crypto)
    target_link_libraries(marketdata INTERFACE ${MDF_BOOST_HEADERS_TARGET})
  else()
    target_compile_definitions(marketdata INTERFACE MDF_NET_ENABLE_BEAST=0)
  endif()
else()
  target_compile_definitions(marketdata INTERFACE MDF_NET_ENABLE_BEAST=0)
endif()

if (MDF_BUILD_APPS)
  add_executable(marketdata-gateway
                 apps/gateway/src/main.cpp
                 apps/gateway/src/binance_live_options.cpp
                 apps/gateway/src/binance_mapper_handlers.cpp
                 apps/gateway/src/binance_live_runner.cpp)
  target_link_libraries(marketdata-gateway PRIVATE marketdata)

  if (EXISTS "${MDF_ORDERBOOK_INCLUDE_DIR}/orderbook/rebuild/mbo_rebuilder.hpp"
      AND EXISTS "${MDF_TRADING_CORE_INCLUDE_DIR}/trading/events/all.hpp")
    add_executable(book-validate
                   tools/book-validate/main.cpp
                   tools/book-validate/book_validate.cpp)
    target_include_directories(book-validate PRIVATE "${MDF_ORDERBOOK_INCLUDE_DIR}")
    target_link_libraries(book-validate PRIVATE marketdata)

    add_executable(event-trace tools/event-trace/main.cpp)
    target_include_directories(event-trace PRIVATE "${MDF_ORDERBOOK_INCLUDE_DIR}")
    target_link_libraries(event-trace PRIVATE marketdata)
  else()
    message(STATUS "Skipping book-validate: orderbook or trading-core headers not found")
  endif()

  if (EXISTS "${MDF_TRADING_CORE_INCLUDE_DIR}/trading/events/all.hpp")
    add_executable(tonglian-mapper-bench
                   tools/bench-tonglian-mapper/main.cpp)
    target_link_libraries(tonglian-mapper-bench
                          PRIVATE marketdata)
  endif()
endif()

if (MDF_BUILD_TESTS)
  enable_testing()

  if (MDF_BUILD_REPLAY_TESTS)
    add_executable(mdf-message-log-smoke
                   tests/replay/message_log_smoke.cpp)
    target_link_libraries(mdf-message-log-smoke PRIVATE marketdata)
    add_test(NAME message_log_smoke COMMAND mdf-message-log-smoke)
  endif()

  add_executable(mdf-binance-feeds-smoke
                 tests/adapters/crypto/binance/binance_feeds_smoke.cpp)
  target_link_libraries(mdf-binance-feeds-smoke PRIVATE marketdata)
  add_test(NAME binance_feeds_smoke COMMAND mdf-binance-feeds-smoke)

  add_executable(mdf-binance-book-ticker-mapper-smoke
                 tests/adapters/crypto/binance/binance_book_ticker_mapper_smoke.cpp)
  target_link_libraries(mdf-binance-book-ticker-mapper-smoke PRIVATE marketdata)
  add_test(NAME binance_book_ticker_mapper_smoke
           COMMAND mdf-binance-book-ticker-mapper-smoke)

  add_executable(mdf-binance-trade-mapper-smoke
                 tests/adapters/crypto/binance/binance_trade_mapper_smoke.cpp)
  target_link_libraries(mdf-binance-trade-mapper-smoke PRIVATE marketdata)
  add_test(NAME binance_trade_mapper_smoke
           COMMAND mdf-binance-trade-mapper-smoke)

  if (EXISTS "${MDF_TRADING_CORE_INCLUDE_DIR}/trading/events/all.hpp")
    add_executable(mdf-cn-stock-tonglian-mapper-smoke
                   tests/adapters/cn/stock/tonglian/mapper_smoke.cpp)
    target_link_libraries(mdf-cn-stock-tonglian-mapper-smoke PRIVATE marketdata)
    add_test(NAME cn_stock_tonglian_mapper_smoke
             COMMAND mdf-cn-stock-tonglian-mapper-smoke)
  else()
    message(STATUS "Skipping cn_stock_tonglian_mapper_smoke: trading-core include dir not found")
  endif()

  add_executable(mdf-websocket-endpoint-smoke
                 tests/net/websocket_endpoint_smoke.cpp)
  target_link_libraries(mdf-websocket-endpoint-smoke PRIVATE marketdata)
  add_test(NAME websocket_endpoint_smoke COMMAND mdf-websocket-endpoint-smoke)

  add_executable(mdf-spsc-ring-smoke tests/runtime/spsc_ring_smoke.cpp)
  target_link_libraries(mdf-spsc-ring-smoke PRIVATE marketdata)
  add_test(NAME spsc_ring_smoke COMMAND mdf-spsc-ring-smoke)

  add_executable(mdf-feed-session-smoke tests/service/feed_session_smoke.cpp)
  target_link_libraries(mdf-feed-session-smoke PRIVATE marketdata)
  add_test(NAME feed_session_smoke COMMAND mdf-feed-session-smoke)

  add_executable(mdf-event-journal-smoke
                 tests/replay/event_journal_smoke.cpp)
  target_link_libraries(mdf-event-journal-smoke PRIVATE marketdata)
  add_test(NAME event_journal_smoke COMMAND mdf-event-journal-smoke)

  add_executable(mdf-checkpoint-smoke tests/replay/checkpoint_smoke.cpp)
  target_link_libraries(mdf-checkpoint-smoke PRIVATE marketdata)
  add_test(NAME checkpoint_smoke COMMAND mdf-checkpoint-smoke)

  add_executable(mdf-trading-phase-tracker-smoke
                 tests/venues/trading_phase_tracker_smoke.cpp)
  target_link_libraries(mdf-trading-phase-tracker-smoke PRIVATE marketdata)
  add_test(NAME trading_phase_tracker_smoke
           COMMAND mdf-trading-phase-tracker-smoke)

  if (EXISTS "${MDF_TRADING_CORE_INCLUDE_DIR}/trading/events/all.hpp")
    add_executable(mdf-tonglian-sequence-gate-smoke
                   tests/adapters/cn/stock/tonglian/sequence_gate_smoke.cpp)
    target_link_libraries(mdf-tonglian-sequence-gate-smoke
                          PRIVATE marketdata)
    add_test(NAME tonglian_sequence_gate_smoke
             COMMAND mdf-tonglian-sequence-gate-smoke)
  endif()

endif()
