cmake_minimum_required(VERSION 3.15)
project(portfolio_cpp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

add_executable(
    portfolio_cpp
    main.cpp
    src/platform/fallback/null_telemetry_collector.cpp
    src/platform/windows/windows_telemetry_collector.cpp
    src/web/api_payloads.cpp
    src/web/app_routes.cpp
    src/web/frontend_assets.cpp
)

target_include_directories(
    portfolio_cpp
    PRIVATE
        include
        third_party/crow/include
        third_party/asio/asio/include
)

target_link_libraries(portfolio_cpp PRIVATE Threads::Threads)

if(WIN32)
    target_link_libraries(portfolio_cpp PRIVATE ws2_32 mswsock wbemuuid ole32 oleaut32 shell32)
endif()
