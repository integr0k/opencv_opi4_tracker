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
# No highgui: the board is headless — the picture is streamed to a PC
# as MJPEG over HTTP (see main.cpp), open it in a browser.
CV_LIBS = -lopencv_core -lopencv_imgproc -lopencv_videoio -lopencv_imgcodecs -lopencv_dnn -lmotrack

TARGET = main
SRCS   = $(wildcard *.cpp)
OBJS   = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS) $(CV_LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

# make run                                        -> default RTSP url
# make run URL=rtsp://192.168.1.82:8080/h264_opus.sdp
# make run URL=... PORT=9000 MODEL=yolo11n.onnx
run: $(TARGET)
	./$(TARGET) "$(URL)" $(PORT) $(MODEL)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all run clean
