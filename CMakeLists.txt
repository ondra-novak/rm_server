cmake_minimum_required(VERSION 3.0)
project (rm_server)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/)

include_directories(BEFORE src/imtjson/src src)
add_compile_options(-std=c++17 -fPIC -Wall -Wextra)

set(USERVER_NO_SSL 1)

add_subdirectory (src/userver  EXCLUDE_FROM_ALL)
add_subdirectory (src/imtjson/src/imtjson  EXCLUDE_FROM_ALL)
add_subdirectory (src/userver_jsonrpc  EXCLUDE_FROM_ALL)
add_subdirectory (src/main) 


