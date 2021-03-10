cmake_minimum_required(VERSION 3.0) 

add_library (pdf
	libmain.cpp pdf_lex.cpp structs.cpp struct_parser.cpp
)
add_executable (testpdf main.cpp)
target_link_libraries (testpdf LINK_PUBLIC
	pdf 
    imtjson
    stdc++fs
    pthread
)
