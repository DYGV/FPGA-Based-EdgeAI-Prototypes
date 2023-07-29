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

add_executable(face_detection_simple face_detection_simple.cpp)
add_executable(face_detection_server face_detection_server.cpp)
add_executable(client client.cpp)

if(CMAKE_BUILD_TYPE MATCHES Debug)
    target_compile_definitions(face_detection_simple PRIVATE DEBUG)
    target_compile_definitions(face_detection_server PRIVATE DEBUG)
    target_compile_definitions(client PRIVATE DEBUG)
elseif(CMAKE_BUILD_TYPE MATCHES Latency)
    target_compile_definitions(client PRIVATE LATENCY)
endif()


set(FACE_DETECTION_LIBS
    vitis_ai_library-facedetect
    vitis_ai_library-dpu_task
    ${OpenCV_LIBRARIES}
    glog
    pthread
    ${Boost_LIBRARIES}
)
target_link_libraries(face_detection_simple ${FACE_DETECTION_LIBS})
target_link_libraries(face_detection_server ${FACE_DETECTION_LIBS})

target_link_libraries(client ${OpenCV_LIBRARIES} ${Boost_LIBRARIES})
