#include <unistd.h>

#include "filedatastorage.h"

using namespace swift;

#ifdef _WIN32
#define OPENFLAGS         O_RDWR|O_CREAT|_O_BINARY
#else
#define OPENFLAGS         O_RDWR|O_CREAT
#endif

#define POS2OFFSET(pos)     (pos.base_offset()<<10)

FileDataStorage::FileDataStorage( const char* filename ) : fd_(0) {
    fd_ = open( filename , OPENFLAGS, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH );
    if( fd_ < 0 ) {
        fd_ = 0;
        print_error( "cannot open the file" );
        return;
    }
    filename_ = strdup( filename );
}

FileDataStorage::~FileDataStorage( ) {
    if( fd_ )
        close( fd_ );
    if( filename_ )
        free( filename_ );
}

size_t FileDataStorage::read( char* buf, size_t len ) {
    return ::read( fd_, buf, len );
}

size_t FileDataStorage::read( off_t pos, char* buf, size_t len ) {
    return pread( fd_, buf, len, pos );
}

size_t FileDataStorage::read( bin64_t pos, char* buf, size_t len ) {
    return pread( fd_, buf, len, POS2OFFSET( pos ) );
}

size_t FileDataStorage::write( const char* buf, size_t len ) {
    return ::write( fd_, buf, len );
}

size_t FileDataStorage::write( off_t pos, const char* buf, size_t len ) {
    return pwrite( fd_, buf, len, pos );
}

size_t FileDataStorage::write( bin64_t pos, const char* buf, size_t len ) {
    return pwrite( fd_, buf, len, POS2OFFSET( pos ) );
}

size_t FileDataStorage::size() {
    return file_size( fd_ );
}

bool FileDataStorage::setSize( size_t len ) {
    return file_resize( fd_, len );
}

bool FileDataStorage::valid( ) {
    return fd_;
}
