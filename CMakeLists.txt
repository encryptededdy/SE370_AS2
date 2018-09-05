cmake_minimum_required(VERSION 3.5)
project(SE370_AS2 C)

set(CMAKE_C_STANDARD 11)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

include_directories(.)

add_executable(NumCores
        dispatchQueue.h
        num_cores.c)

add_executable(Test1
        dispatchQueue.h
        test1.c dispatchQueue.c)
target_link_libraries(Test1 Threads::Threads)

add_executable(Test2
        dispatchQueue.h
        test2.c dispatchQueue.c)
target_link_libraries(Test2 Threads::Threads)

add_executable(Test3
        dispatchQueue.h
        test3.c dispatchQueue.c)
target_link_libraries(Test3 Threads::Threads)

add_executable(Test4
        dispatchQueue.h
        test4.c dispatchQueue.c)
target_link_libraries(Test4 Threads::Threads)

add_executable(Test5
        dispatchQueue.h
        test5.c dispatchQueue.c)
target_link_libraries(Test5 Threads::Threads)

add_executable(TestFor
        dispatchQueue.h
        testFor.c dispatchQueue.c)
target_link_libraries(TestFor Threads::Threads)