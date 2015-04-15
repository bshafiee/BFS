#-O3 produces not debug info! Should be used for performance measurments
#CXXFLAGS =	-O3 -Wall -fmessage-length=0 -D_FILE_OFFSET_BITS=64 -std=c++1y
#-ggdb produces lots of debug info; useful ful backtraces and debugging
CXXFLAGS =	-ggdb -g -Wall -fmessage-length=0 -D_FILE_OFFSET_BITS=64 -std=c++1y

CXXSOURCES = $(wildcard src/*.cpp)
OBJS =	$(CXXSOURCES:%.cpp=%.o)

#defines
DEFINES=

#Includes
LFLAGS=-Ilib/logger
CXXFLAGS+= $(LFLAGS)

#Libraries
LD = -L/usr/local/lib
CXXFLAGS+=$(LD)

#if PROFILE is enabled them -lprofiler should be include and gperftools should be installed
#LIBS = -lpthread -lfuse -lSwift -lPocoNet -lPocoFoundation -lzookeeper_mt -lpfring -lnuma -lgfapi -ltcmalloc -lprofiler
#No profiler
LIBS = -lpthread -lfuse -lSwift -lPocoFoundation -lPocoNet -lzookeeper_mt -lpfring -lnuma -lgfapi -ltcmalloc

TARGET =	BFS
#CXX=clang++
$(TARGET):	$(OBJS)
	$(CXX) $(DEFINES) $(CXXFLAGS) -o $(TARGET) $(OBJS) $(LIBS)
	#gksudo setcap cap_net_admin+eip $(TARGET)

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
