CFLAGS = -O2 -s
LFLAGS = -lpcap
CXX=g++
CXXFLAGS = $(CFLAGS)

default: assniffer

OBJS = sniff.o tcp.o http.o

assniffer: $(OBJS)
	$(CXX) $(LFLAGS) $(CFLAGS) -o assniffer $(OBJS)

clean: 
	-rm *.o *.core *.ncb *.opt *.plg assniffer
