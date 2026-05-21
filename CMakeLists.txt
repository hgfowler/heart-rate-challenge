cmake_minimum_required(VERSION 3.20)

project(
    HeartRateChallenge
    VERSION 0.1.0
    DESCRIPTION "Estimate heart rate from a video using OpenCV"
    LANGUAGES CXX
)

find_package(OpenCV REQUIRED COMPONENTS core imgproc videoio highgui)

add_executable(heart_rate)

target_sources(
    heart_rate
    PRIVATE
        main.cpp
)

target_compile_features(
    heart_rate
    PRIVATE
        cxx_std_17
)

set_target_properties(
    heart_rate
    PROPERTIES
        CXX_EXTENSIONS OFF
)

target_link_libraries(
    heart_rate
    PRIVATE
        ${OpenCV_LIBS}
)