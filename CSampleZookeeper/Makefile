#CXXFLAGS =	-O3 -g -Wall -fmessage-length=0 -std=c++11
CXXFLAGS =	-ggdb -g -Wall -fmessage-length=0 -std=c++11

CXXSOURCES = $(wildcard *.cpp model/*.cpp)
OBJS =		$(CXXSOURCES:%.cpp=%.o)

#Include path
LFLAGS=-Izookeeper
CXXFLAGS+= $(LFLAGS)

#Library path
LD = -Llibs/
CXXFLAGS+=$(LD)

LIBS =-lzookeeper_mt -lpthread

TARGET =	CSampleZookeeper

$(TARGET):	$(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS) $(LIBS)

all:	$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)
