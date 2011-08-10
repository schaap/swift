#include <unistd.h>

#include "fileoffsetdatastorage.h"

using namespace swift;

#ifdef _WIN32
#define OPENFLAGS         O_RDWR|O_CREAT|_O_BINARY
#else
#define OPENFLAGS         O_RDWR|O_CREAT
#endif

#define POS2OFFSET(pos)     (pos.base_offset()<<10)

FileOffsetDataStorage::FileOffsetDataStorage( const char* filename, size_t offset ) : offset_(offset), size_(0), cur_(0), fd_(0), filename_(0) {
    fd_ = open( filename , OPENFLAGS, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH );
    if( fd_ < 0 ) {
        fd_ = 0;
        print_error( "cannot open the file" );
        return;
    }
    filename_ = strdup( filename );
    size_ = file_size( fd_ );
}

FileOffsetDataStorage::~FileOffsetDataStorage( ) {
    if( fd_ )
        close( fd_ );
    if( filename_ )
        free( filename_ );
}

size_t FileOffsetDataStorage::read( char* buf, size_t len ) {
    int ret = read( cur_, buf, len );
    if( ret < 0 )
        return ret;
    cur_ += ret;
    return ret;
}

size_t FileOffsetDataStorage::read( off_t pos, char* buf, size_t len ) {
    if( pos + offset_ + len < size_ )
        return pread( fd_, buf, len, pos + offset_ );
    else {
        int sublen = size_ - ( pos + offset_);
        int ret = pread( fd_, buf, sublen, pos + offset_ );
        if( ret < sublen )
            return ret;
        int ret2 = pread( fd_, buf + sublen, std::min(len - sublen, offset_), 0 );
        if( ret2 < 0 )
            return ret2; // Prefer returning error rather than correct length
        return ret + ret2;
    }
}

size_t FileOffsetDataStorage::read( bin64_t pos, char* buf, size_t len ) {
    return read( POS2OFFSET( pos ), buf, len );
}

size_t FileOffsetDataStorage::write( const char* buf, size_t len ) {
    int ret = write( cur_, buf, len );
    if( ret < 0 )
        return ret;
    cur_ += ret;
    return ret;
}

size_t FileOffsetDataStorage::write( off_t pos, const char* buf, size_t len ) {
    if( pos + offset_ + len < size_ )
        return pwrite( fd_, buf, len, pos + offset_ );
    else {
        int sublen = size_ - (pos + offset_);
        int ret = pwrite( fd_, buf, sublen, pos + offset_ );
        if( ret < sublen )
            return ret;
        int ret2 = pwrite( fd_, buf + sublen, std::min(len - sublen, offset_), 0 );
        if( ret2 < 0 )
            return ret2; // Prefer returning error rather than correct length
        return ret + ret2;
    }
}

size_t FileOffsetDataStorage::write( bin64_t pos, const char* buf, size_t len ) {
    return write( POS2OFFSET(pos), buf, len );
}

size_t FileOffsetDataStorage::size() {
    return size_;
}

bool FileOffsetDataStorage::setSize( size_t len ) {
    bool ret = file_resize( fd_, len );
    if( ret )
        size_ = len;
    return ret;
}

bool FileOffsetDataStorage::valid( ) {
    return fd_;
}
