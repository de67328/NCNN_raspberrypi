cmake_minimum_required(VERSION 3.14)
project(serial_ball_detect LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── OpenCV ──
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs highgui)

# ── NCNN：优先使用 make install 生成的 CMake 包 ──
find_package(ncnn CONFIG QUIET)
if(ncnn_FOUND)
    set(NCNN_TARGET ncnn)
else()
    set(NCNN_DIR "/usr/local" CACHE PATH "ncnn install prefix")
    find_library(NCNN_LIB ncnn PATHS ${NCNN_DIR}/lib ${NCNN_DIR}/lib64 REQUIRED)
    set(NCNN_TARGET ${NCNN_LIB})
endif()

# ── OpenMP ──
find_package(OpenMP REQUIRED)
find_package(Threads REQUIRED)

message(STATUS "OpenCV: ${OpenCV_VERSION}")
message(STATUS "NCNN:   ${NCNN_TARGET}")

# ── Target ──
add_executable(ball_detect
    main.cpp
    detector.cpp
    camera.cpp
    visual.cpp
)

target_include_directories(ball_detect PRIVATE
    ${OpenCV_INCLUDE_DIRS}
    .
)

if(NOT ncnn_FOUND)
    target_include_directories(ball_detect PRIVATE ${NCNN_DIR}/include)
endif()

target_link_libraries(ball_detect
    ${OpenCV_LIBS}
    ${NCNN_TARGET}
    OpenMP::OpenMP_CXX
    Threads::Threads
)

# ── 每次构建都校验并同步最新模型 ──
set(MODEL_PARAM "${CMAKE_SOURCE_DIR}/model/best.param")
set(MODEL_BIN "${CMAKE_SOURCE_DIR}/model/best.bin")
if(NOT EXISTS "${MODEL_PARAM}" OR NOT EXISTS "${MODEL_BIN}")
    message(FATAL_ERROR
        "NCNN model not found. Run scripts/export_ncnn.py before building.")
endif()

add_custom_target(copy_model ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "${CMAKE_CURRENT_BINARY_DIR}/model"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MODEL_PARAM}"
            "${CMAKE_CURRENT_BINARY_DIR}/model/best.param"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MODEL_BIN}"
            "${CMAKE_CURRENT_BINARY_DIR}/model/best.bin"
    COMMENT "Synchronizing NCNN model"
    VERBATIM
)
add_dependencies(ball_detect copy_model)
