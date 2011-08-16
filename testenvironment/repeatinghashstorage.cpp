#include "repeatinghashstorage.h"
#include "../ext/filehashstorage.h"

using namespace swift;

#ifdef _WIN32
#define OPENFLAGS         O_RDWR|O_CREAT|_O_BINARY
#else
#define OPENFLAGS         O_RDWR|O_CREAT
#endif


// TODO: Full review


RepeatingHashStorage::RepeatingHashStorage( const char* filename, unsigned int repeat ) : repeat_(repeat), real_(filename), bmp_(-1), extra_(), internalrepeat_(1), lowlayercount_(0), highlayercount_(1), prevlowlayercount_(0), reallength_(0), bmp_itemsize_(0) {
    if( !repeat )
        repeat_ = 1;

    for( int i = 0; i < 64; i++ )
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

    // internalrepeat_: smallest power of 2 greater than or equal to repeat_
    while( internalrepeat_ < repeat_ ) {
        internalrepeat_ <<= 1;
        highlayercount_++;
    }
    bmp_itemsize_ = highlayercount_ / 8;
    if( bmp_itemsize_ * 8  < highlayercount_ )
        bmp_itemsize_++;

    extra_.setHashCount( internalrepeat_ );

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

bool RepeatingHashStorage::setHashCount( int count ) {
    // assertion: count >= repeat_
    int reallength_ = count / repeat_;
    if( repeat_ * reallength_ < count )
        reallength_++;

    if( file_resize( bmp_, count * bmp_itemsize_ * 2 ) ) {
        print_error( "Could not resize bitmap file" );
        return false;
    }

    int internalcount = 1;
    int layercount = 1;
    layerlength_[0] = count;
    while( internalcount < count ) {
        internalcount <<= 1;
        layercount++;
        layerlength_[layercount] = count / internalcount;
        if( layerlength_[layercount] * internalcount < count )
            layerlength_[layercount]++;
    }

    lowlayercount_ = layercount - highlayercount_;

    if( prevlowlayercount_ < lowlayercount_ ) {
        int layer, item;
        // More lower layers, so move higher layers into lower layers
        int layerdiff = lowlayercount_ - prevlowlayercount_;
        int movelayers = layerdiff;
        for( layer = 0; layer < movelayers; layer++ ) {
            int itemcount = 1 << (movelayers - layer); // Number of items in the layer that could have been added before
            for( item = 0; item < itemcount; item++ ) {
                Sha1Hash hash = extra_.getHash( bin64_t( layer, item ) );
                if( hash != Sha1Hash::ZERO )
                    setHash( bin64_t( prevlowlayercount_ + layer, item ), hash );
            }
        }
        int
        movelayers = highlayercount_ - layerdiff;
        for( layer = 0; layer < movelayers; layer++ ) {
            int itemcount = 1 << (movelayers - layer); 
            int fromlayer = layer + layerdiff;
            for( item  = 0; item < itemcount; item++ ) {
                Sha1Hash hash = extra_.getHash( bin64_t( fromlayer, item ) );
                if( hash != Sha1Hash::ZERO ) {
                    extra_.setHash( bin64_t( layer, item ), hash );
                    extra_.setHash( bin64_t( fromlayer, item ), Sha1Hash::ZERO );
                }
            }
        }
        prevlowlayercount_ = lowlayercount_;
    }
    // NOTE: Downsizing not supported!

    return real_.setHashCount( reallength_ );
}

int RepeatingHashStorage::setHash( bin64_t number, const Sha1Hash& hash ) {
    int layer = number.layer();
    uint64_t count = number.offset();
    if( layerlength_[layer] < count )
        setHashCount( ((count-1) * (1 << layer)) + 1);
    if( layer > lowlayercount_ )
        extra_.setHash( bin64_t( layer - lowlayercount_, count ), hash );
    else {
        uint64_t realcount = count % reallength_;
        uint64_t iter = (count - realcount) / reallength;
        uint64_t iterbyte = iter >> 3;
        char buf[] = {0};
        off_t offset = bin64_t( layer, realcount ) * bmp_itemsize_ + iterbyte;
        pread( bmp_, buf, 1, offset ); // Reads 0 bytes iff beyond file size, so no bother
        ((unsigned char*)buf)[0] |= (unsigned char)1 << (item & 0x7);
        pwrite( bmp_, buf, 1, offset );
        real_.setHash( bin64_t( layer, realcount ), hash );
    }
}

Sha1Hash& RepeatingHashStorage::getHash( bin64_t number ) {
    int layer = number.layer();
    uint64_t count = number.offset();
    if( layerlength_[layer] < count )
        return Sha1Hash::ZERO;
    if( layer > lowlayercount_ )
        return extra_.getHash( bin64_t( layer - lowlayercount_, count ), hash );
    else {
        uint64_t realcount = count % reallength_;
        uint64_t iter = (count - realcount) / reallength;
        uint64_t iterbyte = iter >> 3;
        char buf[] = {0};
        off_t offset = bin64_t( layer, realcount ) * bmp_itemsize_ + iterbyte;
        pread( bmp_, buf, 1, offset ); // Reads 0 bytes iff beyond file size, so no bother
        if( ! ( ((unsigned char*)buf)[0] & ((unsigned char)1 << (iter & 0x7) ) ) )
            return Sha1Hash::ZERO;
        return real_.getHash( bin64_t( layer, realcount ) );
    }
}
