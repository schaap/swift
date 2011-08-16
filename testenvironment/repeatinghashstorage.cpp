#include "repeatinghashstorage.h"
#include "../ext/filehashstorage.h"

using namespace swift;

#ifdef _WIN32
#define OPENFLAGS         O_RDWR|O_CREAT|_O_BINARY
#else
#define OPENFLAGS         O_RDWR|O_CREAT
#endif


// Known and important limitation: the repeated pattern of hashes has always a length (in number of hashes) of a power of 2
// The same goes for the number fo repeats, with a minimum of 2. I.e. the length of a repeating hash storage is always a power of 2.

// Note that depending on the usage pattern the structure adepts. This means that the repetition pattern can change over time.
// Depending on the usage this can mean that data is lost while the structure is still adepting to its final size. (i.e. sequentially filling with hashes)
// For this reason it is best to always try and specify the size of the structure BEFORE writing any data to it.

RepeatingHashStorage::RepeatingHashStorage( const char* filename, unsigned int repeat ) : repeat_(repeat), real_(filename), bmp_(-1), extra_(), lowlayercount_(1), highlayercount_(1), bmp_itemsize_(0), mask_(0) {
    int i;
    if( !repeat )
        repeat_ = 2;

    for( i = 0; i < 64; i++ )
        layerlength_[i] = 0;

    if( !real_.valid() )
        return;

    if( strlen( filename ) >= 1023 ) {
        fprintf( stderr, "filename too long to derive .bitmap file: %s\n", filename );
        return;
    }

    char buf[1024];
    snprintf( buf, 1024, "%s.bitmap", filename );
    bmp_ = open( buf, OPENFLAGS, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH );
    if( bmp_ < 0 ) {
        perror( "Can't create bitmap file\n" );
        return;
    }

    // internalrepeat: smallest power of 2 greater than or equal to repeat_
    int internalrepeat = 2;
    while( internalrepeat < repeat_ ) {
        internalrepeat <<= 1;
        highlayercount_++;
    }
    repeat_ = internalrepeat;

    int layerlength = 1;
    for( i = highlayercount_; i >= 0; i-- ) {
        layerlength_[i] = layerlength;
        layerlength <<= 1;
    }

    bmp_itemsize_ = highlayercount_ / 8;
    if( bmp_itemsize_ * 8  < highlayercount_ )
        bmp_itemsize_++;

    extra_.setHashCount( repeat_ );

    real_.setHashCount( 1 );

    if( !extra_.valid() ) {
        close( bmp_ );
        bmp_ = -1;
    }
}

RepeatingHashStorage::~RepeatingHashStorage() {
    if( bmp_ >= 0 )
        close( bmp_ );
}

bool RepeatingHashStorage::valid() {
    return real_.valid() && extra_.valid() && bmp_ >= 0;
}

// Downsizing is very explicitly not supported
// Arbitrary counts are upsized to the nearest power of 2
bool RepeatingHashStorage::setHashCount( int count ) {
    int prevlowlayercount = lowlayercount_;
    int reallength = 1 << (lowlayercount_ - 1);
    int lowlayercount = lowlayercount_;
    while( reallength * repeat_ < count ) {
        reallength <<= 1;
        lowlayercount++;
    }

    if( lowlayercount == prevlowlayercount )
        return true;

    if( file_resize( bmp_, reallength * bmp_itemsize_ * 2 ) ) {
        print_error( "Could not resize bitmap file" );
        return false;
    }

    if( !real_.setHashCount( reallength ) )
        return false;

    lowlayercount_ = lowlayercount;
    mask_ = reallength - 1;

    int layerlength = 1;
    for( int i = highlayercount_; i >= 0; i-- ) {
        layerlength_[i] = layerlength;
        layerlength <<= 1;
    }

    unsigned int layer, item, itemcount;
    unsigned int layerdiff = lowlayercount_ - prevlowlayercount;
    unsigned int movelayers = std::min(layerdiff, highlayercount_ );
    // Move movelayers lower layers of extra_ to new upper layers of real_ starting with layer prevlowlayercount
    for( layer = 0; layer < movelayers; layer++ ) {
        itemcount = 1 << ((movelayers - layer) - 1); // Number of items in the layer that could have been added before
        for( item = 0; item < itemcount; item++ ) {
            Sha1Hash hash = extra_.getHash( bin64_t( layer, item ) );
            if( hash != Sha1Hash::ZERO ) {
                setHash( bin64_t( prevlowlayercount + layer, item ), hash );
                extra_.setHash( bin64_t( layer, item ), Sha1Hash::ZERO );
            }
        }
    }

    movelayers = highlayercount_ - layerdiff;
    if( movelayers > 0 ) {
        // Move layers in extra_ layerdiff layers down
        for( layer = 0; layer < movelayers; layer++ ) {
            itemcount = 1 << ((movelayers - layer) - 1); 
            int fromlayer = layer + layerdiff;
            for( item  = 0; item < itemcount; item++ ) {
                Sha1Hash hash = extra_.getHash( bin64_t( fromlayer, item ) );
                if( hash != Sha1Hash::ZERO ) {
                    extra_.setHash( bin64_t( layer, item ), hash );
                    extra_.setHash( bin64_t( fromlayer, item ), Sha1Hash::ZERO );
                }
            }
        }
    }

    return true;
}

bool RepeatingHashStorage::setHash( bin64_t number, const Sha1Hash& hash ) {
    int layer = number.layer();
    uint64_t count = number.offset();
    if( layerlength_[layer] <= count )
        setHashCount( ((count-1) * (1 << layer)) + 1);
    if( layer > lowlayercount_ )
        return extra_.setHash( bin64_t( layer - lowlayercount_, count ), hash );
    else {
        uint64_t realcount = count & (mask_ >> layer);
        uint64_t iter = count >> (lowlayercount_ - layer);
        char buf[] = {0};
        off_t offset = bin64_t( layer, realcount ) * bmp_itemsize_ + (iter >> 3);
        pread( bmp_, buf, 1, offset ); // Reads 0 bytes iff beyond file size
        ((unsigned char*)buf)[0] |= (unsigned char)1 << (iter & 0x7);
        if( !real_.setHash( bin64_t( layer, realcount ), hash ) )
            return false;
        pwrite( bmp_, buf, 1, offset );
    }
}

const Sha1Hash& RepeatingHashStorage::getHash( bin64_t number ) {
    int layer = number.layer();
    uint64_t count = number.offset();
    if( layerlength_[layer] <= count )
        return Sha1Hash::ZERO;
    if( layer > lowlayercount_ )
        return extra_.getHash( bin64_t( layer - lowlayercount_, count ) );
    else {
        uint64_t realcount = count & (mask_ >> layer);
        uint64_t iter = count >> (lowlayercount_ - layer);
        char buf[] = {0};
        off_t offset = bin64_t( layer, realcount ) * bmp_itemsize_ + (iter >> 3);
        pread( bmp_, buf, 1, offset ); // Reads 0 bytes iff beyond file size
        if( ! ( ((unsigned char*)buf)[0] & ((unsigned char)1 << (iter & 0x7) ) ) )
            return Sha1Hash::ZERO;
        return real_.getHash( bin64_t( layer, realcount ) );
    }
}
