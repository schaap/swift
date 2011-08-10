#define _XOPEN_SOURCE
#include <time.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <endian.h>

#define quit(x) { perror(x); exit(-1); }

uint64_t fromhex64( char* str, int sz );

int main( int argc, char** argv ) {
    if( argc < 3 ) {
        printf( "Usage: %s in-file out-file-basename\n", argv[0] );
        return -1;
    }

    int input = open( argv[1], O_RDONLY );
    if( input == -1 )
        quit( argv[1] );
    int output = -1;
    char buf[1024];
    char buf2[1024];
    int n, i;
    int p = 0;
    int state = 0;
    struct tm header;
    int version = 0;
    char microsuser[8];
    char microskernel[8];
    char bytesread[8];
    char byteswritten[8];
    char sleepytime[4];
    int ret;

    do {
        n = read( input, buf, 1024 );
        if( n < 0 )
            quit( "reading input" );

        for( i = 0; i < n; i++ ) {
            // Read next byte
            buf2[p++] = buf[i];
            if( buf[i] == '\n' )
                buf2[p-1] = 0;
            else
                continue;

            // Full line read, interpret
            switch( state ) {
                case 0 : // Not in an actual log
                    // Empty line, discard
                    if( strlen( buf2 ) == 0 ) {
                        p = 0;
                        continue;
                    }
                    // Read header
                    if( !strptime( buf2, "%F %T", &header ) ) {
                        printf( "Header \"%s\" could not be converted to a date\n", buf2 );
                        exit( -1 );
                    }
                    // Construct filename and open file
                    snprintf( buf2, 1024, "%s_%02i-%02i-%04i_%02i:%02i:%02i", argv[2], header.tm_mday, header.tm_mon, header.tm_year, header.tm_hour, header.tm_min, header.tm_sec );
                    output = open( buf2, O_CREAT|O_WRONLY, S_IRUSR|S_IWUSR );
                    if( output < 0 )
                        quit( buf2 );
                    // Continue to next state
                    state = 1;
                    version = 0;
                    break;
                case 1 : // Log header read, expect version
                    version = strtol( buf2, NULL, 0 );
                    if( version != 1 ) {
                        printf( "Unknown version: %i\n", version );
                        exit( -1 );
                    }
                    switch( version ) {
                        case 1 : // OK version, continue to next state
                            state = 2;
                            break;
                        default:
                            printf( "Unknown version: %i\n", version );
                            exit( -1 );
                    }
                    break;
                case 2 : // Reading log version 1, expect entry or log header
                    // Empty line, discard, but definitely end of log
                    if( strlen( buf2 ) == 0 ) {
                        close( output );
                        output = -1;
                        p = 0;
                        state = 0;
                        continue;
                    }
                    // Normal line? Let's see
                    if( strlen( buf2 ) == 40 ) {
                        ret = sscanf( buf2, "a%8cb%8cc%8cd%8ce%4c", microsuser, microskernel, bytesread, byteswritten, sleepytime );
                        if( ret == 5 ) {
                            snprintf( buf2, 1024, "%i %i %i %i %i\n", (int)fromhex64(microsuser,8), (int)fromhex64(microskernel,8), (int)fromhex64(bytesread,8), (int)fromhex64(byteswritten,8), (int)fromhex64(sleepytime,4) );
                            if( write( output, buf2, strlen(buf2) ) != strlen(buf2) )
                                quit( "writing output" );
                            p = 0;
                            continue;
                        }
                    }
                    // Not a normal line. New header?
                    if( !strptime( buf2, "%F %T", &header ) ) {
                        printf( "Unexpected input: %s\n", buf2 );
                        exit( -1 );
                    }
                    close( output );
                    // Construct filename and open file
                    snprintf( buf2, 1024, "%s_%02i-%02i-%04i_%02i:%02i:%02i", argv[2], header.tm_mday, header.tm_mon, header.tm_year, header.tm_hour, header.tm_min, header.tm_sec );
                    output = open( buf2, O_CREAT|O_WRONLY, S_IRUSR|S_IWUSR );
                    if( output < 0 )
                        quit( buf2 );
                    // Continue to next state
                    state = 1;
                    version = 0;
                    break;
                default :
                    printf( "Failure! State has reached %i\n", state );
                    exit( -1 );
            }
            // Line done, next line
            p = 0;
        }
    } while( n );

    close( output );

    buf2[p] = 0;
    if( strlen( buf2 ) )
        printf( "Warning! Garbage at end of input: \"%s\"\n", buf2 );

    return 0;
}

uint64_t fromhex64( char* str, int sz ) {
    uint8_t u8;
    uint16_t u16;
    uint32_t u32;
    uint64_t u64;

    uint8_t* p = NULL;
    switch( sz ) {
        case 2 :
            p = &u8;
            break;
        case 4 :
            p = (uint8_t*)(&u16);
            break;
        case 8 :
            p = (uint8_t*)(&u32);
            break;
        case 16 :
            p = (uint8_t*)(&u64);
            break;
        default :
            printf( "Error! Called fromhex64 with sz = %i\n", sz );
            exit(-1);
    }

    int i = 0;

    while( i < sz ) {
        *p++ = ((((uint8_t)str[i]) & ~64 ) << 4) | (((uint8_t)str[i+1]) & ~64);
        i += 2;
    }

    switch( sz ) {
        case 2 :
            return u8;
        case 4 :
            return be16toh( u16 );
        case 8 :
            return be32toh( u32 );
        case 16 :
            return be64toh( u64 );
    }
}
