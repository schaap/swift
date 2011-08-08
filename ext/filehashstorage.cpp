#include <unistd.h>

#include "filehashstorage.h"
#include "../compat.h"
#include "../swift.h"
#include "../sha1.h"

using namespace swift;

#ifdef _WIN32
#define OPENFLAGS         O_RDWR|O_CREAT|_O_BINARY
#else
#define OPENFLAGS         O_RDWR|O_CREAT
#endif

FileHashStorage::FileHashStorage( const char* filename ) :
    HashStorage(), hash_fd_(0), hashes_(0)
{
    hash_fd_ = open( filename, OPENFLAGS, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH );
    if( hash_fd_ < 0 ) {
        hash_fd_ = 0;
        print_error( "cannot open hash file" );
        return;
    }
}

FileHashStorage::~FileHashStorage( ) {
    if( hashes_ ) {
        memory_unmap( hash_fd_, hashes_, hashes_size_ );
        hashes_ = NULL;
        hashes_size_ = 0;
    }

    if( hash_fd_ )
        close( hash_fd_ );
}

bool FileHashStorage::valid( ) {
    return hash_fd_;
}

bool FileHashStorage::setHashCount( int count ) {
    hashes_size_ = sizeof(Sha1Hash) * 2 * count;
    if( file_size( hash_fd_ ) != hashes_size_ ) {
        if( file_resize( hash_fd_, hashes_size_ ) ) {
            hashes_size_ = 0;
            print_error( "could not resize hash file" );
            return false;
        }
    }
    hashes_ = (Sha1Hash*) memory_map( hash_fd_, hashes_size_ );
    if( !hashes_ ) {
        hashes_size_ = 0;
        print_error( "mmap failed" );
        return false;
    }
    return true;
}

bool FileHashStorage::setHash( bin64_t number, const Sha1Hash& hash ) {
    if( hashes_ ) {
        hashes_[number] = hash;
        return true;
    }

    if( file_seek( hash_fd_, number * sizeof( Sha1Hash ) ) < 0 ) {
        print_error( "seeking failed" );
        return false;
    }
    if( write( hash_fd_, (const void*) &hash, sizeof( Sha1Hash ) ) != sizeof( Sha1Hash ) )
        return false;
    return true;
}

const Sha1Hash& FileHashStorage::getHash( bin64_t number ) {
    if( hashes_ )
        return hashes_[number];

    if( file_seek( hash_fd_, number * sizeof( Sha1Hash ) ) < 0 ) {
        print_error( "seeking failed" );
        return Sha1Hash::ZERO;
    }
    Sha1Hash hash;
    if( read( hash_fd_, &hash, sizeof( Sha1Hash ) ) != sizeof( Sha1Hash ) )
        return Sha1Hash::ZERO;
    Sha1Hash& ret = hash;
    return ret;
}

void FileHashStorage::hashLeftRight( bin64_t root ) {
    if( hashes_ )
        hashes_[root] = Sha1Hash(hashes_[root.left()], hashes_[root.right()]);
    else
        setHash( root, Sha1Hash( getHash( root.left() ), getHash( root.right() ) ) );
}
