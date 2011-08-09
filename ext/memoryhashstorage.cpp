#include <unistd.h>

#include "memoryhashstorage.h"
#include "../compat.h"
#include "../swift.h"
#include "../sha1.h"

using namespace swift;

MemoryHashStorage::MemoryHashStorage( ) : hashes_(1)
{
}

MemoryHashStorage::~MemoryHashStorage( ) {
}

bool MemoryHashStorage::valid( ) {
    return true;
}

bool MemoryHashStorage::setHashCount( int count ) {
    hashes_.resize( 2*count );
    return hashes_.capacity() == 2*count;
}

bool MemoryHashStorage::setHash( bin64_t number, const Sha1Hash& hash ) {
    if( (int) number > hashes_.capacity() ) {
        hashes_.resize( number+1 );
        if( (int) number > hashes_.capacity() )
            return false;
    }
    hashes_[number] = hash;
    return true;
}

const Sha1Hash& MemoryHashStorage::getHash( bin64_t number ) {
    if( (int) number > hashes_.capacity() )
        return Sha1Hash::ZERO;
    return hashes_[number];
}

void MemoryHashStorage::hashLeftRight( bin64_t root ) {
    hashes_[root] = Sha1Hash(hashes_[root.left()], hashes_[root.right()]);
}
