cmake_minimum_required(VERSION 3.5)
project(mrs)

set(CXX_FLAGS
    -g
)

add_executable(${PROJECT_NAME} server.cc)
target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${PROJECT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/mrs
)
target_link_libraries(${PROJECT_NAME} pthread)
