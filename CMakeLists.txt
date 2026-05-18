cmake_minimum_required(VERSION 3.15)
project(portfolio_cpp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

# ── Auto-fetch third-party deps if missing ────────────────────────────────────
include(FetchContent)

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/third_party/crow/include/crow.h")
    message(STATUS "Crow not found — fetching single header from GitHub")
    file(DOWNLOAD
            "https://github.com/CrowCpp/Crow/releases/latest/download/crow_all.h"
            "${CMAKE_SOURCE_DIR}/third_party/crow/include/crow.h"
            SHOW_PROGRESS
            STATUS CROW_DOWNLOAD_STATUS
    )
    list(GET CROW_DOWNLOAD_STATUS 0 CROW_DOWNLOAD_CODE)
    if(NOT CROW_DOWNLOAD_CODE EQUAL 0)
        message(FATAL_ERROR "Failed to download Crow. Check your internet connection.")
    endif()
endif()

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/third_party/asio/asio/include/asio.hpp")
    message(STATUS "ASIO not found — fetching from GitHub")
    FetchContent_Declare(asio
            GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
            GIT_TAG        master
            GIT_SHALLOW    TRUE
            SOURCE_DIR     "${CMAKE_SOURCE_DIR}/third_party/asio"
    )
    FetchContent_Populate(asio)
endif()

# ── Executable ────────────────────────────────────────────────────────────────
add_executable(
        portfolio_cpp
        main.cpp
        src/web/api_payloads.cpp
        src/web/app_routes.cpp
        src/web/frontend_assets.cpp
        # Headers — listed for IDE visibility only, not compiled
        include/portfolio/telemetry_models.hpp
        include/portfolio/platform/telemetry_collector.hpp
        include/portfolio/web/api_payloads.hpp
        include/portfolio/web/app_routes.hpp
        include/portfolio/web/frontend_assets.hpp
)

if(WIN32)
    target_sources(portfolio_cpp PRIVATE
            src/platform/windows/windows_telemetry_collector.cpp
    )
elseif(APPLE)
    target_sources(portfolio_cpp PRIVATE
            src/platform/macos/macos_telemetry_collector.cpp
    )
else()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/platform/linux/linux_telemetry_collector.cpp")
        target_sources(portfolio_cpp PRIVATE src/platform/linux/linux_telemetry_collector.cpp)
    else()
        target_sources(portfolio_cpp PRIVATE src/platform/fallback/null_telemetry_collector.cpp)
        target_compile_definitions(portfolio_cpp PRIVATE TELEMETRY_FALLBACK)
    endif()
endif()

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
elseif(APPLE)
    target_link_libraries(portfolio_cpp PRIVATE "-framework CoreFoundation" "-framework IOKit")
else()
    # ARM Linux requires libatomic for std::atomic; gracefully skip on x86
    find_library(ATOMIC_LIB atomic)
    if(ATOMIC_LIB)
        target_link_libraries(portfolio_cpp PRIVATE atomic)
    endif()
    target_link_libraries(portfolio_cpp PRIVATE rt)
    target_compile_definitions(portfolio_cpp PRIVATE _GNU_SOURCE ASIO_STANDALONE)
endif()