CPPFLAGS=-I.. -DDEBUG -g
LDFLAGS=-I..

TESTS=tests/fileoffsetdatastoragetest tests/repeatinghashstoragetest

all: swift statconverter

statconverter: statconverter.o
	$(CXX) $(CPPFLAGS) $(LDFLAGS) -lm $^ -o $@

statconverter.o: statconverter.cpp
	$(CXX) -c $(CPPFLAGS) $< -o $@

swift.o: swift.cpp
	$(CXX) -c $(CPPFLAGS) $< -o $@

fileoffsetdatastorage.o: fileoffsetdatastorage.cpp
	$(CXX) -c $(CPPFLAGS) $< -o $@

%.o: ../%.cpp
	$(CXX) -c $(CPPFLAGS) $< -o $@

ext/%.o : ../ext/%.cpp
	mkdir -p ext/
	$(CXX) -c $(CPPFLAGS) $< -o $@

swift: swift.o sha1.o compat.o sendrecv.o send_control.o hashtree.o bin64.o bins.o channel.o datagram.o transfer.o httpgw.o ext/memoryhashstorage.o ext/filehashstorage.o ext/filedatastorage.o fileoffsetdatastorage.o repeatinghashstorage.o
	$(CXX) $(CPPFLAGS) $(LDFLAGS) -lpthread $^ -o $@

clean:
	rm -f *.o swift ext/*.o statconverter tests/*.o $(TESTS)
	[ ! -d ext ] || rmdir --ignore-fail-on-non-empty ext/

test: $(TESTS)
	for a in $(TESTS); do $(TESTPREFIX) $$a; done

tests/fileoffsetdatastoragetest: tests/fileoffsetdatastoragetest.o fileoffsetdatastorage.o compat.o
	$(CXX) $(CPPFLAGS) $(LDFLAGS) $(GTESTFLAGS) -lgtest -lpthread $^ -o $@

tests/repeatinghashstoragetest: tests/repeatinghashstoragetest.o repeatinghashstorage.o hashtree.o ext/filehashstorage.o ext/memoryhashstorage.o bin64.o compat.o sha1.o bins.o ext/filedatastorage.o
	$(CXX) $(CPPFLAGS) $(LDFLAGS) $(GTESTFLAGS) -lgtest -lpthread $^ -o $@

tests/%.o: tests/%.cpp
	$(CXX) -c $(CPPFLAGS) $(GTESTFLAGS) $< -o $@
