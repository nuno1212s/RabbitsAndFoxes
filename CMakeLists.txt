cmake_minimum_required(VERSION 3.17)
project(Trabalho_2 C)

set(CMAKE_C_STANDARD 11)

add_executable(Trabalho_2 main.c matrix_utils.c matrix_utils.h rabbitsandfoxes.c rabbitsandfoxes.h linkedlist.c linkedlist.h movements.c movements.h threads.c threads.h)
target_link_libraries(Trabalho_2 pthread)