#include "../repeatinghashstorage.h"

#include <gtest/gtest.h>

// Define REFERENCE as 1 for reference test using MemoryHashStorage, 2 for reference test using FileHashStorage
#define REFERENCE 2

using namespace swift;

const Sha1Hash hashes[] = {
    Sha1Hash( "1" ),
    Sha1Hash( "2" ),
    Sha1Hash( "3" ),
    Sha1Hash( "4" ),
    Sha1Hash( "5" ),
    Sha1Hash( "6" ),
    Sha1Hash( "7" ),
    Sha1Hash( "8" )
};

const Sha1Hash calcHash( bin64_t n, int basesize ) {
    if( n.layer() == 0 )
        return hashes[n.offset() % basesize];
    else {
        Sha1Hash res = Sha1Hash( calcHash( n.left(), basesize ), calcHash( n.right(), basesize ) );
        return res;
    }
}

bool checkFilled( HashStorage& rhs, bool* filled, int repeat, int basesize ) {
    int i;
    for( i = 0; i < repeat * basesize * 2 - 1; i++ ) {
        if( filled[bin64_t((uint64_t)i)] ) {
            if( rhs.getHash( bin64_t((uint64_t)i) ) != calcHash( bin64_t((uint64_t)i), basesize ) ) {
                printf( "bin64_t(%d, %d) should have been filled with %s but contained %s\n", bin64_t((uint64_t)i).layer(), bin64_t((uint64_t)i).offset(), calcHash( bin64_t((uint64_t)i), basesize ).hex().c_str(), rhs.getHash( bin64_t((uint64_t)i) ).hex().c_str() );
                return false;
            }
        }
        else {
            if( rhs.getHash( bin64_t((uint64_t)i) ) != Sha1Hash::ZERO ) {
                printf( "bin64_t(%d, %d) should not have been filled but contained %s\n", bin64_t((uint64_t)i).layer(), bin64_t((uint64_t)i).offset(), rhs.getHash( bin64_t((uint64_t)i) ).hex().c_str() );
                return false;
            }
        }
    }
    return true;
}

TEST(RepeatingHashStorage, SetSizeAndSequentialFillBottomUp) {
    int i, j, repeat, basesize, layersize, layer;
    repeat = 2;
    while( repeat <= 16 ) {
        basesize = 1;
        while( basesize <= 8 ) {
            bool filled[repeat*basesize*2];
            for( i = 0; i < repeat * basesize * 2; i++ )
                filled[i] = false;
            unlink( "tests/teststorage.mhash" );

#if REFERENCE == 1
            MemoryHashStorage rhs;
#elif REFERENCE == 2
            FileHashStorage rhs( "tests/teststorage.mhash" );
#else
            RepeatingHashStorage rhs( "tests/teststorage.mhash", repeat );
#endif
            ASSERT_TRUE( rhs.valid() );
            ASSERT_TRUE( checkFilled( rhs, filled, repeat, basesize ) );
            ASSERT_TRUE( rhs.setHashCount( repeat * basesize ) );
            ASSERT_TRUE( rhs.valid() );
            ASSERT_TRUE( checkFilled( rhs, filled, repeat, basesize ) );
            layersize = repeat * basesize;
            layer = 0;
            while( layersize ) {
                for( i = 0; i < layersize; i++ ) {
                    ASSERT_TRUE( rhs.setHash( bin64_t( layer, i ), calcHash( bin64_t( layer, i ), basesize ) ) );
                    filled[bin64_t( layer, i )] = true;
                    if( !checkFilled( rhs, filled, repeat, basesize ) ) {
                        printf( "Just added bin64_t(%d, %d) (repeat = %d, basesize = %d)\n", layer, i, repeat, basesize );
                        ASSERT_TRUE( false );
                    }
                }
                layersize >>= 1;
                layer++;
            }
            for( i = 0; i < repeat; i++ ) {
                for( j = 0; j < basesize; j++ )
                    ASSERT_TRUE( rhs.getHash( bin64_t( 0, j ) ) == rhs.getHash( bin64_t( 0, i * basesize + j ) ) );
            }

            basesize <<= 1;
        }
        repeat <<= 1;
    }
}

TEST(RepeatingHashStorage, SetSizeAndSequentialFillTopDown) {
    int i, j, repeat, basesize, layersize, layer;
    repeat = 2;
    while( repeat <= 16 ) {
        basesize = 1;
        while( basesize <= 8 ) {
            bool filled[repeat*basesize*2];
            for( i = 0; i < repeat * basesize * 2; i++ )
                filled[i] = false;
            unlink( "tests/teststorage.mhash" );

#if REFERENCE == 1
            MemoryHashStorage rhs;
#elif REFERENCE == 2
            FileHashStorage rhs( "tests/teststorage.mhash" );
#else
            RepeatingHashStorage rhs( "tests/teststorage.mhash", repeat );
#endif
            ASSERT_TRUE( rhs.valid() );
            ASSERT_TRUE( checkFilled( rhs, filled, repeat, basesize ) );
            ASSERT_TRUE( rhs.setHashCount( repeat * basesize ) );
            ASSERT_TRUE( rhs.valid() );
            ASSERT_TRUE( checkFilled( rhs, filled, repeat, basesize ) );
            layersize = repeat * basesize;
            layer = 0;
            while( layersize ) {
                layersize >>= 1;
                layer++;
            }
            layer--;
            layersize = 1;
            while( layer >= 0 ) {
                for( i = 0; i < layersize; i++ ) {
                    ASSERT_TRUE( rhs.setHash( bin64_t( layer, i ), calcHash( bin64_t( layer, i ), basesize ) ) );
                    filled[bin64_t( layer, i )] = true;
                    if( !checkFilled( rhs, filled, repeat, basesize ) ) {
                        printf( "Just added bin64_t(%d, %d) (repeat = %d, basesize = %d)\n", layer, i, repeat, basesize );
                        ASSERT_TRUE( false );
                    }
                }
                layersize <<= 1;
                layer--;
            }

            basesize <<= 1;
        }
        repeat <<= 1;
    }
}

TEST(RepeatingHashStorage, SequentialFillTopDown) {
    int i, j, repeat, basesize, layersize, layer;
    repeat = 2;
    while( repeat <= 16 ) {
        basesize = 1;
        while( basesize <= 8 ) {
            bool filled[repeat*basesize*2];
            for( i = 0; i < repeat * basesize * 2; i++ )
                filled[i] = false;
            unlink( "tests/teststorage.mhash" );

#if REFERENCE == 1
            MemoryHashStorage rhs;
#elif REFERENCE == 2
            FileHashStorage rhs( "tests/teststorage.mhash" );
#else
            RepeatingHashStorage rhs( "tests/teststorage.mhash", repeat );
#endif
            Sha1Hash l0 = Sha1Hash("1");
            Sha1Hash l1 = Sha1Hash(l0,l0);
            Sha1Hash l2 = Sha1Hash(l1,l1);
            Sha1Hash l3 = Sha1Hash(l2,l2);
            Sha1Hash l4 = Sha1Hash(l3,l3);
            ASSERT_TRUE( rhs.valid() );
            ASSERT_TRUE( checkFilled( rhs, filled, repeat, basesize ) );
            ASSERT_TRUE( rhs.valid() );
            ASSERT_TRUE( checkFilled( rhs, filled, repeat, basesize ) );
            layersize = repeat * basesize;
            layer = 0;
            while( layersize ) {
                layersize >>= 1;
                layer++;
            }
            layer--;
            layersize = 1;
            while( layer >= 0 ) {
                for( i = 0; i < layersize; i++ ) {
                    ASSERT_TRUE( rhs.setHash( bin64_t( layer, i ), calcHash( bin64_t( layer, i ), basesize ) ) );
                    filled[bin64_t( layer, i )] = true;
                    if( !checkFilled( rhs, filled, repeat, basesize ) ) {
                        printf( "Just added bin64_t(%d, %d) (repeat = %d, basesize = %d)\n", layer, i, repeat, basesize );
                        ASSERT_TRUE( false );
                    }
                }
                layersize <<= 1;
                layer--;
            }

            basesize <<= 1;
        }
        repeat <<= 1;
    }
}

TEST(RepeatingHashStorage, SetSizeRandomFill) {
    int i, j, repeat, basesize, totalsize, layer;
    repeat = 2;
    while( repeat <= 16 ) {
        basesize = 1;
        while( basesize <= 8 ) {
            bool filled[repeat*basesize*2];
            for( i = 0; i < repeat * basesize * 2; i++ )
                filled[i] = false;
            unlink( "tests/teststorage.mhash" );

#if REFERENCE == 1
            MemoryHashStorage rhs;
#elif REFERENCE == 2
            FileHashStorage rhs( "tests/teststorage.mhash" );
#else
            RepeatingHashStorage rhs( "tests/teststorage.mhash", repeat );
#endif
            ASSERT_TRUE( rhs.valid() );
            ASSERT_TRUE( checkFilled( rhs, filled, repeat, basesize ) );
            ASSERT_TRUE( rhs.setHashCount( repeat * basesize ) );
            ASSERT_TRUE( rhs.valid() );
            ASSERT_TRUE( checkFilled( rhs, filled, repeat, basesize ) );
            totalsize = repeat * basesize - 1;
            int toBeFilled[totalsize];
            for( i = 0; i < totalsize; i++ )
                toBeFilled[i] = i;
            while( totalsize ) {
                int next = random() % totalsize; // biased, but /care
                ASSERT_TRUE( rhs.setHash( bin64_t( (uint64_t)toBeFilled[next] ), calcHash( bin64_t( (uint64_t)toBeFilled[next] ), basesize ) ) );
                filled[bin64_t( (uint64_t)toBeFilled[next] )] = true;
                if( !checkFilled( rhs, filled, repeat, basesize ) ) {
                    printf( "Just added bin64_t(%d, %d) (repeat = %d, basesize = %d)\n", bin64_t( (uint64_t)toBeFilled[next] ).layer(), bin64_t( (uint64_t)toBeFilled[next] ).offset(), repeat, basesize );
                    ASSERT_TRUE( false );
                }
                toBeFilled[next] = toBeFilled[--totalsize];
            }

            basesize <<= 1;
        }
        repeat <<= 1;
    }
}

int main( int argc, char** argv ) {
    testing::InitGoogleTest( &argc, argv );
    return RUN_ALL_TESTS();
}
