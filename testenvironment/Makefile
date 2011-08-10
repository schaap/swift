CPPFLAGS=-I.. -DDEBUG -g
LDFLAGS=-I.. -lpthread

all: swift

swift.o: swift.cpp
	$(CXX) -c $(CPPFLAGS) $< -o $@

fileoffsetdatastorage.o: fileoffsetdatastorage.cpp
	$(CXX) -c $(CPPFLAGS) $< -o $@

%.o: ../%.cpp
	$(CXX) -c $(CPPFLAGS) $< -o $@

ext/%.o : ../ext/%.cpp
	mkdir -p ext/
	$(CXX) -c $(CPPFLAGS) $< -o $@

swift: swift.o sha1.o compat.o sendrecv.o send_control.o hashtree.o bin64.o bins.o channel.o datagram.o transfer.o httpgw.o ext/memoryhashstorage.o ext/filehashstorage.o ext/filedatastorage.o fileoffsetdatastorage.o
	$(CXX) $(CPPFLAGS) $(LDFLAGS) $^ -o $@

clean:
	rm -f *.o swift ext/*.o
