#ifndef STORAGE_H
#define STORAGE_H

#include "bin64.h"
#include "compat.h"
#include "sha1hash.h"

namespace swift {

    class DataStorage {
    public:
        DataStorage (const Sha1Hash& id, size_t size);
        virtual size_t    ReadData (bin64_t pos,uint8_t** buf) {return -1;}
        virtual size_t    WriteData (bin64_t pos, uint8_t* buf, size_t len) {return -1;};
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

        static HashStorage* NONE;
    };

}

#endif
