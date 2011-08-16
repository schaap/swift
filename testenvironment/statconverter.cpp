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
#include <math.h>

#include "testenvironment/compat.h"

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
    int n, i, j;
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
    int microsuser_cuml = 0;
    int microskernel_cuml = 0;
    int bytesrx_cuml = 0;
    int bytestx_cuml = 0;
    int slicecnt = 0;
    int userwindow[100];
    int kernelwindow[100];
    int rxwindow[100];
    int txwindow[100];
    int useravg;
    int kernelavg;
    int rxavg;
    int txavg;
    int windowpointer;
    int userprev;
    int kernelprev;

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
                    if( (ret =sscanf( buf2, "%4u-%2u-%2u %2u:%2u:%2u", (unsigned int*)&header.tm_year, (unsigned int*)&header.tm_mon, (unsigned int*)&header.tm_mday, (unsigned int*)&header.tm_hour, (unsigned int*)&header.tm_min, (unsigned int*)&header.tm_sec ) )!= 6 ) {
                        printf( "Header \"%s\" could not be converted to a date: %i\n", buf2, ret );
                        exit( -1 );
                    }
                    // Construct filename and open file
                    snprintf( buf2, 1024, "%s_%02i-%02i-%04i_%02i:%02i:%02i", argv[2], header.tm_mday, header.tm_mon, header.tm_year, header.tm_hour, header.tm_min, header.tm_sec );
                    output = open( buf2, O_CREAT|O_WRONLY, S_IRUSR|S_IWUSR );
                    if( output < 0 )
                        quit( buf2 );
                    printf( "Writing log to %s (time in GMT)\n", buf2 );
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
                            printf( "- Log version 1:\n" );
                            printf( "- timeslice# usertime usertime(cml) usertime%%/sec kerneltime kerneltime(cml) kerneltime%%/sec bytesrx bytesrx(cml) bytesrx/sec bytestx bytestx(cml) bytestx/sec sleeptime\n" );
                            printf( "- times are absolute, per timeslice and in us; timeslice is 10000ms (100Hz); cml=cumulative; X/sec by trailing average\n" );
                            state = 2;
                            microsuser_cuml = 0;
                            microskernel_cuml = 0;
                            bytesrx_cuml = 0;
                            bytestx_cuml = 0;
                            slicecnt = 0;
                            bzero( userwindow, 100*sizeof(int) );
                            bzero( kernelwindow, 100*sizeof(int) );
                            bzero( txwindow, 100*sizeof(int) );
                            bzero( rxwindow, 100*sizeof(int) );
                            windowpointer = 0;
                            userprev = 0;
                            kernelprev = 0;
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
                    if( strlen( buf2 ) == 41 ) {
                        ret = sscanf( buf2, "a%8cb%8cc%8cd%8ce%4c", microsuser, microskernel, bytesread, byteswritten, sleepytime );
                        if( ret == 5 ) {
                            userwindow[windowpointer] = (int)fromhex64(microsuser,8) - userprev;
                            userprev = (int)fromhex64(microsuser,8);
                            microsuser_cuml += userwindow[windowpointer];
                            kernelwindow[windowpointer] = (int)fromhex64(microskernel,8) - kernelprev;
                            kernelprev = (int)fromhex64(microskernel,8);
                            microskernel_cuml += kernelwindow[windowpointer];
                            rxwindow[windowpointer] = (int)fromhex64(bytesread,8);
                            bytesrx_cuml += rxwindow[windowpointer];
                            txwindow[windowpointer] = (int)fromhex64(byteswritten,8);
                            bytestx_cuml += txwindow[windowpointer];
                            rxavg = 0;
                            txavg = 0;
                            useravg = 0;
                            kernelavg = 0;
                            for( j = 0; j < 100; j++ ) {
                                rxavg += rxwindow[j];
                                txavg += txwindow[j];
                                useravg += userwindow[j];
                                kernelavg += kernelwindow[j];
                            }
                            useravg = ceilf( (float)useravg / 10000.0 ); // (useravg / 1000000) * 100%
                            kernelavg = ceilf( (float)kernelavg / 10000.0);
                            snprintf( buf2, 1024, "%i %i %i %i %i %i %i %i %i %i %i %i %i %i\n", slicecnt++, userwindow[windowpointer], microsuser_cuml, useravg, kernelwindow[windowpointer], microskernel_cuml, kernelavg, rxwindow[windowpointer], bytesrx_cuml, rxavg, txwindow[windowpointer], bytestx_cuml, txavg, (int)fromhex64(sleepytime,4) );
                            windowpointer = (windowpointer + 1) % 100;
                            if( write( output, buf2, strlen(buf2) ) != strlen(buf2) )
                                quit( "writing output" );
                            p = 0;
                            continue;
                        }
                    }
                    // Not a normal line. New header?
                    if( (ret =sscanf( buf2, "%4u-%2u-%2u %2u:%2u:%2u", (unsigned int*)&header.tm_year, (unsigned int*)&header.tm_mon, (unsigned int*)&header.tm_mday, (unsigned int*)&header.tm_hour, (unsigned int*)&header.tm_min, (unsigned int*)&header.tm_sec ) )!= 6 ) {
                        printf( "Unexpected input: %s\n", buf2 );
                        exit( -1 );
                    }
                    close( output );
                    // Construct filename and open file
                    snprintf( buf2, 1024, "%s_%02i-%02i-%04i_%02i:%02i:%02i", argv[2], header.tm_mday, header.tm_mon, header.tm_year, header.tm_hour, header.tm_min, header.tm_sec );
                    output = open( buf2, O_CREAT|O_WRONLY, S_IRUSR|S_IWUSR );
                    if( output < 0 )
                        quit( buf2 );
                    printf( "Writing log to %s (time in GMT)\n", buf2 );
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
