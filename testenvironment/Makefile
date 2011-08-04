CPPFLAGS=-O2 -I..
LDFLAGS=-I..

all: swift

swift.o: swift.cpp
	$(CXX) -c $(CPPFLAGS) $< -o $@

%.o: ../%.cpp
	$(CXX) -c $(CPPFLAGS) $< -o $@

swift: swift.o sha1.o compat.o sendrecv.o send_control.o hashtree.o bin64.o bins.o channel.o datagram.o transfer.o httpgw.o
	$(CXX) $(CPPFLAGS) $(LDFLAGS) $^ -o $@

clean:
	rm -f *.o swift
