cmake_minimum_required(VERSION 3.15)
project(face_detection)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-O2 -Wall")

find_package(OpenCV REQUIRED)

add_executable(face_detection_simple face_detection_simple.cpp)
add_executable(face_detection_seq face_detection_seq.cpp)
add_executable(face_detection_server face_detection_server.cpp)
add_executable(client client.cpp)

set(DEP_LIBS
    ${OpenCV_LIBRARIES}
    pthread
)

set(FACE_DETECTION_LIBS
    vitis_ai_library-facedetect
    vitis_ai_library-dpu_task
    ${DEP_LIBS}
)

target_link_libraries(face_detection_simple ${FACE_DETECTION_LIBS})
target_link_libraries(face_detection_seq ${FACE_DETECTION_LIBS})
target_link_libraries(face_detection_server ${FACE_DETECTION_LIBS})

target_link_libraries(client ${DEP_LIBS})
