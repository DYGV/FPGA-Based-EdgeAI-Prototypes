cmake_minimum_required(VERSION 3.15)
project(Vitis-AI-Accel)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-O2")

find_package(OpenCV REQUIRED)
include_directories(
    ${OpenCV_INCLUDE_DIRS}
)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost 1.77.0 COMPONENTS system serialization)
include_directories(${Boost_INCLUDE_DIRS})

add_executable(pose_estimation_simple pose_estimation_simple.cpp)
add_executable(pose_estimation_seq pose_estimation_seq.cpp)
add_executable(pose_estimation_server pose_estimation_server.cpp)
add_executable(client client.cpp)

set(POSE_ESTIMATION_LIBS
    vitis_ai_library-openpose
    vitis_ai_library-dpu_task
    ${OpenCV_LIBRARIES}
    glog
    pthread
    ${Boost_LIBRARIES}
)

target_link_libraries(pose_estimation_simple ${POSE_ESTIMATION_LIBS})
target_link_libraries(pose_estimation_seq ${POSE_ESTIMATION_LIBS})
target_link_libraries(pose_estimation_server ${POSE_ESTIMATION_LIBS})
target_link_libraries(client ${OpenCV_LIBRARIES} ${Boost_LIBRARIES} pthread)
