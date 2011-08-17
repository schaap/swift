#include <unistd.h>

#include "memoryhashstorage.h"
#include "../compat.h"
#include "../swift.h"
#include "../sha1.h"

using namespace swift;

MemoryHashStorage::MemoryHashStorage( ) : hashes_(0)
{
}

MemoryHashStorage::~MemoryHashStorage( ) {
}

bool MemoryHashStorage::valid( ) {
    return true;
}

bool MemoryHashStorage::setSize( int number ) {
    int oldcap = hashes_.size();
    hashes_.resize( number );
    for( int i = oldcap; i < hashes_.capacity(); i++ )
        hashes_[i] = Sha1Hash::ZERO;
    return hashes_.size() >= number;
}

bool MemoryHashStorage::setHashCount( int count ) {
    return setSize( 2*count );
}

bool MemoryHashStorage::setHash( bin64_t number, const Sha1Hash& hash ) {
    if( (int) number >= hashes_.size() ) {
        if( !setSize( number+1 ) )
            return false;
    }
    hashes_[number] = hash;
    return true;
}

const Sha1Hash& MemoryHashStorage::getHash( bin64_t number ) {
    if( (int) number >= hashes_.size() )
        return Sha1Hash::ZERO;
    return hashes_[number];
}

void MemoryHashStorage::hashLeftRight( bin64_t root ) {
    hashes_[root] = Sha1Hash(hashes_[root.left()], hashes_[root.right()]);
}
