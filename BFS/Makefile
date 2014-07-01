CXXFLAGS =	-O2 -g -Wall -fmessage-length=0 -D_FILE_OFFSET_BITS=64 -std=c++11

CXXSOURCES = $(wildcard *.cpp model/*.cpp)
OBJS =		$(CXXSOURCES:%.cpp=%.o)

#Includes
LFLAGS=-Ilib/json -Ilib/libSwift/include/swift -Ilib/poco/include -I/usr/local/include/fuse
CXXFLAGS+= $(LFLAGS)

#Libraries
LD = -Llib/poco/lib -Llib/libSwift/lib -L/usr/local/lib
CXXFLAGS+=$(LD)

LIBS = -pthread -lfuse -lSwift -lPocoNet -lPocoFoundation

TARGET =	BFS
CXX=clang++
$(TARGET):	$(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS) $(LIBS)

all:	$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)
