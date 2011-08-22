#define _BSD_SOURCE
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include "compat.h"
#include "../compat.h"

#ifdef _WIN32
#define OPENFLAGS         O_RDWR|O_CREAT|_O_BINARY
#else
#define OPENFLAGS         O_RDWR|O_CREAT
#endif

using namespace swift;

int main( int argc, char** argv ) {
    if( argc < 3 ) {
        printf( "Usage: %s outputfile size\n", argv[0] );
        printf( "Prints semi-non-trivial data to a file: at each 4th byte (0, 3, 7, ...) it prints a 32-bit counter (0, 1, 2, ...) in big-endian byte order\n" );
        printf( "- outputfile : the file to write to\n" );
        printf( "- size : the desired size of the file (will be rounded up to a multiple of 4)\n" );
        return -1;
    }

    long int n = strtol( argv[2], NULL, 10 );
    if( n < 1 ) {
        printf( "Positive size in bytes expected, got %s\n", argv[2] );
        return -1;
    }
    if( n & 0x3 ) {
        n = ( n & ~0X3 ) + 4;
        printf( "Warning: size was given as %s, which is not a multiple of 4. %li bytes will be written instead.\n", argv[2], n );
    }
    if( n > (long int)16 * 1024 * 1024 * 1024 ) {
        printf( "Fake data counter is 32 bits, meaning it can count to 4G and, printing 4 bytes for each count, can generate a maximum file size of 16G\n" );
        return -1;
    }

    int f = open( argv[1], OPENFLAGS, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH );
    if( f < 0 ) {
        perror( argv[1] );
        return -1;
    }
    file_resize( f, n );

    uint32_t i, j;
    int left, done;
    char buf[4096];
    for( i = 0; i < n / 4; i += 1024 ) {
        if( !( i & 0x3FFFF ) )
            printf( "Status: %liM written\n", (long int)( i / ( 1 << 18 ) ) );
        for( j = 0; j < 1024 && j + i < n / 4; j++ )
            *(((uint32_t*) buf)+j) = htobe32( j+i );
        done = pwrite( f, buf, 4096, (i << 2) );
        left = 4096 - done;
        while( left > 0 ) {
            done += pwrite( f, buf + done, left, (i << 2) + left );
            left = 4096 - done;
        }
        if( left < 0 ) {
            perror( "writing" );
            return -2;
        }
        if( i == 0xFFFFFFFF - 1023 ) {
            printf( "Status: 16384M written\n" );
            break; // Prevent infinite loop on generating 16G
        }
    }
    if( !( i & 0x3FFFF ) )
        printf( "Status: %liM written\n", (long int)( i / ( 1 << 18 ) ) );

    long int size = file_size( f );
    if( size != n )
        printf( "Warning: size was supposed to be %li, but turned out to be %li.\n", n, size );

    return 0;
}
