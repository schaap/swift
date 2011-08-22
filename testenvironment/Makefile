# Supported targets:
# - all
# - clean
# - test

# When adding things, be sure to:
# - Add build targets where necessary
# - Add object targets for files other than ../ext/%.cpp (ext/%.o) ../%.cpp (%.o) or tests/%.cpp (tests/%.o)
# - Update "all" if needed
# - Update "clean" if needed
# - Update "TESTS" if needed

CPPFLAGS=-I.. -DDEBUG -g
LDFLAGS=-I..

# Add tests to be run on "make test" here, be sure to make test targets for them as well
TESTS=tests/fileoffsetdatastoragetest tests/repeatinghashstoragetest

# "make all"

all: swift statconverter genfakedata checkfakedata

# Actual targets

statconverter: statconverter.o
	$(CXX) $(CPPFLAGS) $(LDFLAGS) -lm $^ -o $@

swift: swift.o sha1.o compat.o sendrecv.o send_control.o hashtree.o bin64.o bins.o channel.o datagram.o transfer.o httpgw.o ext/memoryhashstorage.o ext/filehashstorage.o ext/filedatastorage.o fileoffsetdatastorage.o repeatinghashstorage.o
	$(CXX) $(CPPFLAGS) $(LDFLAGS) -lpthread $^ -o $@

genfakedata: genfakedata.o compat.o
	$(CXX) $(CPPFLAGS) $(LDFLAGS) $^ -o $@

checkfakedata: checkfakedata.o compat.o
	$(CXX) $(CPPFLAGS) $(LDFLAGS) $^ -o $@

# Test targets

tests/fileoffsetdatastoragetest: tests/fileoffsetdatastoragetest.o fileoffsetdatastorage.o compat.o
	$(CXX) $(CPPFLAGS) $(LDFLAGS) $(GTESTFLAGS) -lgtest -lpthread $^ -o $@

tests/repeatinghashstoragetest: tests/repeatinghashstoragetest.o repeatinghashstorage.o hashtree.o ext/filehashstorage.o ext/memoryhashstorage.o bin64.o compat.o sha1.o bins.o ext/filedatastorage.o
	$(CXX) $(CPPFLAGS) $(LDFLAGS) $(GTESTFLAGS) -lgtest -lpthread $^ -o $@

# Object files from testenvironment/

statconverter.o: statconverter.cpp
	$(CXX) -c $(CPPFLAGS) $< -o $@

swift.o: swift.cpp
	$(CXX) -c $(CPPFLAGS) $< -o $@

fileoffsetdatastorage.o: fileoffsetdatastorage.cpp
	$(CXX) -c $(CPPFLAGS) $< -o $@

genfakedata.o: genfakedata.cpp
	$(CXX) -c $(CPPFLAGS) $< -o $@

checkfakedata.o: checkfakedata.cpp
	$(CXX) -c $(CPPFLAGS) $< -o $@

# Object files from libswift

%.o: ../%.cpp
	$(CXX) -c $(CPPFLAGS) $< -o $@

# Object files from libswift/ext/

ext/%.o : ../ext/%.cpp
	mkdir -p ext/
	$(CXX) -c $(CPPFLAGS) $< -o $@

# Object files from testenvironment/tests/

tests/%.o: tests/%.cpp
	$(CXX) -c $(CPPFLAGS) $(GTESTFLAGS) $< -o $@

# Misc

clean:
	rm -f *.o swift ext/*.o statconverter tests/*.o $(TESTS) tests/teststorage.mhash tests/teststorage.mhash.bitmap genfakedata checkfakedata
	[ ! -d ext ] || rmdir --ignore-fail-on-non-empty ext/

test: $(TESTS)
	for a in $(TESTS); do $(TESTPREFIX) $$a $(TESTPOSTFIX); done

.PHONY: all test clean
