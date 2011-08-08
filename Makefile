CPPFLAGS=-O2 -I. -g

all: swift

swift: swift.o sha1.o compat.o sendrecv.o send_control.o hashtree.o bin64.o bins.o channel.o datagram.o transfer.o httpgw.o ext/filehashstorage.o
	g++ -I. *.o ext/*.o -o swift

clean:
	rm -f *.o ext/*.o swift
