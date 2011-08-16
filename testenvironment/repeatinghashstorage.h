#ifndef REPEATINGHASHSTORAGE_H
#define REPEATINGHASHSTORAGE_H

#include "../storage.h"
#include "../compat.h"
#include "../bin64.h"
#include "../ext/filehashstorage.h"
#include "../ext/memoryhashstorage.h"

namespace swift {
    class RepeatingHashStorage : public HashStorage {
    protected:
        unsigned int repeat_;
        FileHashStorage real_;
        MemoryHashStorage extra_;
        int bmp_;
        
    public:
        RepeatingHashStorage( const char* filename, unsigned int repeat );

        virtual bool valid();
        virtual bool setHashCount( int count );
        virtual bool setHash( bin64_t number, const Sha1Hash& hash );
        virtual const Sha1Hash& getHash( bin64_t number );
    };
}

#endif
