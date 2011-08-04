#ifndef SHA1HASH_H
#define SHA1HASH_H

#include <string.h>

namespace swift {
    /** SHA-1 hash, 20 bytes of data */
    struct Sha1Hash {
        uint8_t    bits[20];

        Sha1Hash() { memset(bits,0,20); }
        /** Make a hash of two hashes (for building Merkle hash trees). */
        Sha1Hash(const Sha1Hash& left, const Sha1Hash& right);
        /** Hash an old plain string. */
        Sha1Hash(const char* str, size_t length=-1);
        Sha1Hash(const uint8_t* data, size_t length);
        /** Either parse hash from hex representation of read in raw format. */
        Sha1Hash(bool hex, const char* hash);
        
        std::string    hex() const;
        bool    operator == (const Sha1Hash& b) const
            { return 0==memcmp(bits,b.bits,SIZE); }
        bool    operator != (const Sha1Hash& b) const { return !(*this==b); }
        const char* operator * () const { return (char*) bits; }
        
        const static Sha1Hash ZERO;
        const static size_t SIZE;
    };
}

#endif
