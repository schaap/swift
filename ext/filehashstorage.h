#ifndef FILEHASHSTORAGE_H
#define FILEHASHSTORAGE_H

#include "../swift.h"

namespace swift {

    class FileHashStorage : public HashStorage {
    private:
        int hash_fd_;
        Sha1Hash* hashes_;
        int hashes_size_;

    public:
        FileHashStorage( const char* filename );
        ~FileHashStorage();

        virtual bool valid();
        virtual bool setHashCount( int count );
        virtual bool setHash( bin64_t number, const Sha1Hash& hash );
        virtual const Sha1Hash& getHash( bin64_t number );
        virtual void hashLeftRight( bin64_t root );
    };

}

#endif
