cmake_minimum_required(VERSION 3.15)
project(pose_estimation)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-O2 -Wall")

find_package(OpenCV REQUIRED)

add_executable(pose_estimation_simple pose_estimation_simple.cpp)
add_executable(pose_estimation_seq pose_estimation_seq.cpp)
add_executable(pose_estimation_server pose_estimation_server.cpp)
add_executable(client client.cpp)

set(DEP_LIBS
    ${OpenCV_LIBRARIES}
    pthread
)

set(POSE_ESTIMATION_LIBS
    vitis_ai_library-openpose
    vitis_ai_library-dpu_task
    ${DEP_LIBS}
)

target_link_libraries(pose_estimation_simple ${POSE_ESTIMATION_LIBS})
target_link_libraries(pose_estimation_seq ${POSE_ESTIMATION_LIBS})
target_link_libraries(pose_estimation_server ${POSE_ESTIMATION_LIBS})
target_link_libraries(client ${DEP_LIBS})
