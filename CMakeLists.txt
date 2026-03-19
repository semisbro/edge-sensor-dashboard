cmake_minimum_required(VERSION 3.15)
project(portfolio_cpp)

# === VCPKG INTEGRATION (this is the key missing part) ===
set(CMAKE_TOOLCHAIN_FILE "C:/Users/Kirsc/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Crow (this pulls in Asio automatically too)
find_package(Crow CONFIG REQUIRED)

add_executable(portfolio_cpp main.cpp)   # change "main.cpp" if your file has a different name

target_link_libraries(portfolio_cpp PRIVATE Crow::Crow ws2_32 mswsock)