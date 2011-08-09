#ifndef MEMORYHASHSTORAGE_H
#define MEMORYHASHSTORAGE_H

#include <vector>
#include "../swift.h"

namespace swift {

    class MemoryHashStorage : public HashStorage {
    private:
        std::vector<Sha1Hash> hashes_;

    public:
        MemoryHashStorage();
        ~MemoryHashStorage();

        virtual bool valid();
        virtual bool setHashCount( int count );
        virtual bool setHash( bin64_t number, const Sha1Hash& hash );
        virtual const Sha1Hash& getHash( bin64_t number );
        virtual void hashLeftRight( bin64_t root );
    };

}

#endif
