cmake_minimum_required(VERSION 3.14)
project(serial_ball_detect LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── OpenCV ──
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs highgui)

# ── NCNN ──
set(NCNN_DIR "/usr/local" CACHE PATH "ncnn install prefix")
find_library(NCNN_LIB ncnn PATHS ${NCNN_DIR}/lib ${NCNN_DIR}/lib64 REQUIRED)

# ── OpenMP ──
find_package(OpenMP REQUIRED)

message(STATUS "OpenCV: ${OpenCV_VERSION}")
message(STATUS "NCNN:   ${NCNN_LIB}")

# ── Target ──
add_executable(ball_detect
    main.cpp
    detector.cpp
    camera.cpp
    visual.cpp
)

target_include_directories(ball_detect PRIVATE
    ${OpenCV_INCLUDE_DIRS}
    ${NCNN_DIR}/include
    .
)

target_link_libraries(ball_detect
    ${OpenCV_LIBS}
    ${NCNN_LIB}
    OpenMP::OpenMP_CXX
)

# ── 拷贝模型 ──
file(COPY ${CMAKE_SOURCE_DIR}/../model DESTINATION ${CMAKE_BINARY_DIR})
