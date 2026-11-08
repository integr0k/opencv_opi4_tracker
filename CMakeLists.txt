cmake_minimum_required(VERSION 3.16)
project(opencv_opi4_tracker CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Arch/Omarchy package ships its config in /usr/lib/cmake/opencv5
if(NOT OpenCV_DIR AND EXISTS "/usr/lib/cmake/opencv5/OpenCVConfig.cmake")
  set(OpenCV_DIR "/usr/lib/cmake/opencv5")
endif()

# No highgui: headless board, output is an MJPEG HTTP stream.
find_package(OpenCV REQUIRED COMPONENTS core imgproc videoio imgcodecs dnn)

add_executable(main main.cpp)
target_include_directories(main PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(main PRIVATE ${OpenCV_LIBS})
