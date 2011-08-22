#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include "compat.h"
#include "../compat.h"

using namespace swift;

int main( int argc, char** argv ) {
    if( argc < 2 ) {
        printf( "Usage: %s filename\n", argv[0] );
        printf( "Checks the file for containing correct fake data (see genfakedata for description)\n" );
        return -1;
    }

    int f = open( argv[1], O_RDONLY );
    if( f < 0 ) {
        perror( argv[1] );
        return -1;
    }

    size_t size = file_size( f );
    if( size < 0 ) {
        perror( argv[1] );
        close( f );
        return -1;
    }
    if( size > (long int)16 * 1024 * 1024 * 1024 ) {
        printf( "Invalid data: generate fake data can't be larger than 16G\n" );
        close( f );
        return -1;
    }

    char buf[4096];
    uint32_t j, k;
    size_t i, l, left, done, ret, todo;
    k = 0;
    for( i = 0; i < size; i += 4096 ) {
        todo = std::min( size - i, (size_t)4096 );
        left = todo;
        if( !( i & 0xFFFFF ) )
            printf( "Status: %liM checked\n", (long int)( i / ( 1 << 20 ) ) );
        done = pread( f, buf, left, i );
        left -= done;
        while( left > 0 ) {
            ret += pread( f, buf + done, left, i + done );
            if( ret <  0 ) {
                perror( "reading more" );
                close( f );
                return -2;
            }
            done += ret;
            left -= ret;
        }
        for( l = 0; l < todo; l += 4 ) {
            j = be32toh( *((uint32_t*) (buf + l)) );
            if( j != k ) {
                printf( "Invalid data: at offset %li the number %i was found, while %i was expected\n", (long int)i + l, (int)j, (int)k );
                close( f );
                return 1;
            }
            k++;
        }
    }
    if( !( i & 0xFFFFF ) )
        printf( "Status: %liM checked\n", (long int)( i / ( 1 << 20 ) ) );
    
    close( f );

    return 0;
}
