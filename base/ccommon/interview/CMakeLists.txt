# cmake_minimum_required(VERSION 3.16)
# project(tritum)

# set(CMAKE_CXX_STANDARD 17)

# include_directories(common)
# include_directories(thirdparty)

# list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/thirdparty/cmake") ## 添加find<module>.cmake寻找的路径
# list(APPEND CMAKE_PREFIX_PATH "${PROJECT_SOURCE_DIR}/thirdparty/") ## 添加config<module>.cmake寻找的路径

# add_subdirectory(app)

add_executable(tritum
    main.cpp
)



