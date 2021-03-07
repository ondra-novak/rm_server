cmake_minimum_required(VERSION 3.0) 

add_executable (rm_server main.cpp rmrpcfsys.cpp)
target_link_libraries (rm_server LINK_PUBLIC 
    userver_jsonrpc 
    imtjson
    userver 
    stdc++fs
    pthread
)
