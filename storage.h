#ifndef STORAGE_H
#define STORAGE_H

#include "bin64.h"
#include "compat.h"
#include "sha1hash.h"

namespace swift {

    class DataStorage {
    public:
        //DataStorage( const Sha1Hash& id, size_t size ); //?
        /// read from start or where last non-positional write or read ended
        virtual size_t read( char* buf, size_t len ) = 0;
        /// read from given position
        virtual size_t read( off_t pos, char* buf, size_t len ) = 0;
        /// read from given position
        virtual size_t read( bin64_t pos, char* buf, size_t len ) = 0;
        /// write to start or where last non-positional write or read ended
        virtual size_t write( const char* buf, size_t len) = 0;
        /// write to given position
        virtual size_t write( off_t pos, const char* buf, size_t len ) = 0;
        /// write to given position
        virtual size_t write( bin64_t pos, const char* buf, size_t len ) = 0;
        virtual size_t size() = 0;
        virtual bool setSize( size_t len ) = 0;
        virtual bool valid() = 0;
    };

    class HashStorage {
    public:
        virtual bool setHashCount( int count ) = 0;
        virtual bool setHash( bin64_t number, const Sha1Hash& hash ) = 0;
        virtual const Sha1Hash& getHash( bin64_t number ) = 0;
        virtual bool valid() = 0;

        virtual void hashLeftRight( bin64_t root ) {
            setHash( root, Sha1Hash( getHash( root.left() ), getHash( root.right() ) ) );
        }
    };

}

#endif
