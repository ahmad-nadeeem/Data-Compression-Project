CXX = g++
CXXFLAGS = -Wall -O2 -I./include
TARGET = bzip2_impl
SOURCES = src/main.cpp src/rle.cpp src/bwt.cpp src/mtf.cpp src/huffman.cpp src/config.cpp 

all:
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET)

clean:
	rm -f $(TARGET) *.o

windows:
	x86_64-w64-mingw32-g++ $(CXXFLAGS) $(SOURCES) -o $(TARGET).exe
