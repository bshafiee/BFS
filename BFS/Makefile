CXXFLAGS =	-O2 -g -Wall -fmessage-length=0 -D_FILE_OFFSET_BITS=64 -std=c++11

CXXSOURCES = $(wildcard *.cpp model/*.cpp)
OBJS =		$(CXXSOURCES:%.cpp=%.o)

#Includes
LFLAGS=-Ilib/json -Ilib/libSwift/include/swift -Ilib/poco/include
CXXFLAGS+= $(LFLAGS)

#Libraries
LD = -Llib/poco/lib -Llib/libSwift/lib
CXXFLAGS+=$(LD)

LIBS =`pkg-config fuse --cflags --libs` -lSwift -lPocoNet -lPocoFoundation

TARGET =	BFS

$(TARGET):	$(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS) $(LIBS)

all:	$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)
