cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 20)

add_executable(main comparators.cpp  lexer.cpp  mython.cpp  object.cpp  object_holder.cpp  object_holder_test.cpp  object_test.cpp  parse.cpp  parse_test.cpp  statement.cpp  statement_test.cpp lexer_test.cpp)