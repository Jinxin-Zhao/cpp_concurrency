cmake_minimum_required(VERSION 3.14)
project(threads)

set(CMAKE_CXX_STANDARD 17)


aux_source_directory(. SRC_LIST)

link_directories(/usr/lib /usr/local/lib)
add_executable(threads ${SRC_LIST}
       ) #${SRC_ALLOCATE_LIST} ${SRC_FLOW_LIST}
target_link_libraries(threads pthread)