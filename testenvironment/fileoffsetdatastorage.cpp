#include <unistd.h>

#include "fileoffsetdatastorage.h"

using namespace swift;

#ifdef _WIN32
#define OPENFLAGS         O_RDWR|O_CREAT|_O_BINARY
#else
#define OPENFLAGS         O_RDWR|O_CREAT
#endif

#define POS2OFFSET(pos)     (pos.base_offset()<<10)

FileOffsetDataStorage::FileOffsetDataStorage( const char* filename, size_t offset, unsigned int repeat ) : offset_(offset), size_(0), fullsize_(0), cur_(0), fd_(0), filename_(0), repeat_(repeat) {
    fd_ = open( filename , OPENFLAGS, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH );
    if( fd_ < 0 ) {
        fd_ = 0;
        print_error( "cannot open the file" );
        return;
    }
    if( !repeat_ )
        repeat_ = 1;
    filename_ = strdup( filename );
    size_ = file_size( fd_ );
    fullsize_ = repeat_ * size_;
}

FileOffsetDataStorage::FileOffsetDataStorage( int f, size_t offset, unsigned int repeat ) : offset_(offset), size_(0), fullsize_(0), cur_(0), fd_(f), filename_(0), repeat_(repeat) {
    if( fd_ < 0 ) {
        fd_ = 0;
        print_error( "file is not open" );
        return;
    }
    if( !repeat_ )
        repeat_ = 1;
    filename_ = strdup( "[none]" );
    size_ = file_size( fd_ );
    fullsize_ = repeat_ * size_;
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

#define offsetOperation( op ) \
    int opos = (pos + offset_) % size_; \
    if( opos + len < size_ ) { \
        /* opos<size_ by definition, opos+len<size_ by test: no need to cut the operation in two halves */ \
        if( pos + len < fullsize_ ) \
            return op( fd_, buf, len, opos ); \
        else \
            return op( fd_, buf, fullsize_ - pos, opos ); \
    } \
    /* opos<size_ by definition, but opos+len>=size_ by test: we need to cut the operation in two halves */ \
    int sublen = size_ - opos; \
    int ret = op( fd_, buf, sublen, opos ); \
    if( ret < sublen ) \
        return ret; \
    size_t morelen; \
    if( pos + len <= fullsize_ ) \
        morelen = len - sublen; \
    else \
        morelen = fullsize_ - ( pos + sublen ); \
    size_t bufoff = sublen; \
    while( morelen > size_ ) { \
        /* note that it could be more efficient to just copy after the first full read, but for more correct IO characteristics every full size_ is read */ \
        ret = op( fd_, buf + bufoff, size_, 0 ); \
        if( ret < size_ ) { \
            if( ret < 0 ) \
                return ret; \
            return bufoff + ret; \
        } \
        bufoff += size_; \
        morelen -= size_; \
    } \
    ret = op( fd_, buf + bufoff, morelen, 0 ); \
    if( ret < 0 ) \
        return ret; \
    return bufoff + ret;

size_t FileOffsetDataStorage::read( off_t pos, char* buf, size_t len ) {
    offsetOperation( pread )
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
    offsetOperation( pwrite )
}

size_t FileOffsetDataStorage::write( bin64_t pos, const char* buf, size_t len ) {
    return write( POS2OFFSET(pos), buf, len );
}

size_t FileOffsetDataStorage::size() {
    return fullsize_;
}

bool FileOffsetDataStorage::setSize( size_t len ) {
    size_t flen = len / repeat_;
    if( flen * repeat_ < len )
        flen++;
    int ret = file_resize( fd_, flen );
    if( !ret ) {
        size_ = flen;
        fullsize_ = len;
    }
    return !ret;
}

bool FileOffsetDataStorage::valid( ) {
    return fd_;
}
