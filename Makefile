#CXXFLAGS =	-O3 -Wall -fmessage-length=0 -D_FILE_OFFSET_BITS=64 -std=c++1y
CXXFLAGS =	-ggdb -g -Wall -fmessage-length=0 -D_FILE_OFFSET_BITS=64 -std=c++1y
CXXSOURCES = $(wildcard src/*.cpp)
OBJS =	$(CXXSOURCES:%.cpp=%.o)

#defines
DEFINES=-DBFS_ZERO=1

#Includes
LFLAGS=-Ilib/logger
CXXFLAGS+= $(LFLAGS)

#Libraries
LD = -L/usr/local/lib
CXXFLAGS+=$(LD)

LIBS = -lpthread -lfuse -lSwift -lPocoNet -lPocoFoundation -lzookeeper_mt -lpfring -lnuma

TARGET =	BFS
#CXX=clang++
$(TARGET):	$(OBJS)
	$(CXX) $(DEFINES) $(CXXFLAGS) -o $(TARGET) $(OBJS) $(LIBS)
	gksudo setcap cap_net_admin+eip $(TARGET)

$(OBJS):
	$(CXX) -MMD $(CXXFLAGS) -c -o $@ $(@:.o=.cpp) $(LIBS)
	@mv -f $(@:.o=.d) $(@:.o=.d.tmp)
	@sed -e 's|.*:|$@:|' < $(@:.o=.d.tmp) > $(@:.o=.d)
	@sed -e 's/.*://' -e 's/\\$$//' < $(@:.o=.d.tmp) | fmt -1 | \
	sed -e 's/^ *//' -e 's/$$/:/' >> $(@:.o=.d)
	@rm -f $(@:.o=.d.tmp)

all:	$(TARGET)
-include $(OBJS:.o=.d)

clean:
	rm -f $(OBJS) $(TARGET) $(OBJS:.o=.d)
