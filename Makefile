# OpenCV 5 is installed system-wide (Arch/Omarchy package "opencv").
# Headers: /usr/include/opencv5, pkg-config name: opencv5

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
CPPFLAGS += $(shell pkg-config --cflags opencv5)
MOTRACK_PREFIX ?= $(CURDIR)/motrack/build/install
CPPFLAGS += -I$(MOTRACK_PREFIX)/include
LDFLAGS += -L$(MOTRACK_PREFIX)/lib -Wl,-rpath,$(MOTRACK_PREFIX)/lib

# Link only the modules we actually use.
# NOTE: `pkg-config --libs opencv5` pulls in every module, including
# opencv_viz, which fails to link because VTK isn't installed. Listing
# modules explicitly avoids that.
CV_LIBS = -lopencv_core -lopencv_imgproc -lopencv_videoio -lopencv_highgui -lopencv_imgcodecs -lopencv_dnn -lmotrack

TARGET = main
SRCS   = $(wildcard *.cpp)
OBJS   = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS) $(CV_LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

# make run                                   -> local camera 0
# make run URL=http://192.168.1.82:8080/video -> network camera (IP Webcam)
run: $(TARGET)
	./$(TARGET) $(URL)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all run clean
