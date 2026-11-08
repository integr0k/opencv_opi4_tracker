cmake_minimum_required(VERSION 3.16)
project(openCV_cam CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Arch/Omarchy package ships its config in /usr/lib/cmake/opencv5
if(NOT OpenCV_DIR AND EXISTS "/usr/lib/cmake/opencv5/OpenCVConfig.cmake")
  set(OpenCV_DIR "/usr/lib/cmake/opencv5")
endif()

find_package(OpenCV REQUIRED COMPONENTS core imgproc videoio highgui imgcodecs)

add_executable(main main.cpp)
target_include_directories(main PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(main PRIVATE ${OpenCV_LIBS})
