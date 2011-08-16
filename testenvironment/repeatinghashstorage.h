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
        /// Number of repeats, power of 2, >= 2
        unsigned int repeat_;
        /// The underlying repeated pattern of hashes, length of real_ is a power of 2 and >= 1
        FileHashStorage real_;
        /// File descriptor to the file with the bitmaps for the hashes (contains 1 bit for each hash for each repetition)
        int bmp_;
        /// The extra hashes to finish the tree above the repeated pattern, length of extra_ is repeat_
        MemoryHashStorage extra_;
        /// Number of layers inside real_
        unsigned int lowlayercount_;
        /// Number of layers inside extra_
        unsigned int highlayercount_;
        /// The size of the bitmap for one repeated hash, in bytes; == ceil(repeat/8)
        unsigned int bmp_itemsize_;
        /// The mask to be used for calculating repetition number and offset inside a repeated layer; == (1 << (lowlayercount_ - 1)) - 1
        uint64_t mask_;
        /// The list of lengths for each layer
        unsigned int layerlength_[64];
        
    public:
        RepeatingHashStorage( const char* filename, unsigned int repeat );
        ~RepeatingHashStorage();

        virtual bool valid();
        virtual bool setHashCount( int count );
        virtual bool setHash( bin64_t number, const Sha1Hash& hash );
        virtual const Sha1Hash& getHash( bin64_t number );
    };
}

#endif
