/*
 *  swift.cpp
 *  swift the multiparty transport protocol
 *  Test environment wrapper for libswift.
 *
 *  Created by Victor Grishchenko on 2/15/10.
 *  Forked by Thomas Schaap on 4/8/11.
 *  Copyright 2010 Delft University of Technology. All rights reserved.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <pthread.h>

#include "../compat.h"
#include "compat.h"
#include "swift.h"
#include "fileoffsetdatastorage.h"
#include "repeatinghashstorage.h"


using namespace swift;


#define quit(...) {fprintf(stderr,__VA_ARGS__); exit(1); }


// Global variable to correctly stop execution on SIGINT
volatile bool end_loop = false;
// And a simple signal catcher to catch SIGINT
void catch_signal( int signum ) {
    if( signum == SIGINT )
        end_loop = true;
}

int readPositiveIntArg( ) {
    char* ret;
    errno = 0;
    int val = strtol( optarg, &ret, 10 );
    if( errno )
        quit( "invalid integer: %s\n", strerror( errno ) );
    if( *ret != 0 )
        quit( "integer contains invalid characters: %c\n", (int)(*ret) );
    if( ret <= 0 )
        quit( "expected integer larger than 0\n" );
    return val;
}

// statargs should be of type char**[] with *(statargs[0]) being the filename to write
// statistics to.
void* statisticsThread( void* statargs ) {
    // The statistics file is a (semi-)binary data file containing a human readable
    // date marker on the first line, the version (an integer) on the second line
    // and statistics data after that. Lines are separated by '\n'. Data records
    // are described per version and are appended without extra newline. Any
    // newlines 'between' records are actually part of the records themselves.
    //
    // The version of the statistics is written to the file and should be used
    // to verify how the data is encoded. Below a list of versions that have been
    // used and are in use.
    //
    // Note on hex|64: this encoding was chosen because it allows checking the
    // data for obvious corruptions, avoids control characters, and can still be
    // encoded efficiently. The actual encoding: convert to big endian byte order
    // and store each byte X of the data as two bytes: ((X >> 4) | 64) and
    // ((X & 15) | 64).
    //
    // Version history (only append, DO NOT delete or modify old versions):
    // - 1 (@100Hz):    (was 1000Hz, but that uses about 4 to 5 percent CPU)
    // -- 1 byte 'a'
    // -- 32 bits microseconds user CPU time, cumulative (hex|64 encoded)
    // -- 1 byte 'b'
    // -- 32 bits microseconds kernel CPU time, cumulative (hex|64 encoded)
    // -- 1 byte 'c'
    // -- 32 bits number of bytes read on watched interface (hex|64 encoded)
    // -- 1 byte 'd'
    // -- 32 bits number of bytes writte to watched interface (hex|64 encoded)
    // -- 1 byte 'e'
    // -- 16 bits number of microseconds slept during last period (hex|64 encoded)
    // -- 1 byte '\n'

    // TODO: Add full CPU usage (not only process usage)
    
    // Extract parameters
    char* filename = *(((char***)statargs)[0]);
    
    // Prepare all measurement tools
    struct rusage ru1;
    struct rusage ru2;
    uint64_t rx1, rx2, tx1, tx2;
    char buf[1024];
    char buf2[1024];
    ssize_t n;
    struct timeval tv1;
    struct timeval tv2;
    uint32_t microsuser, microskernel, bytesread, byteswritten;
    useconds_t sleepytime;
    uint16_t sleepytime16;
    uint8_t* microsuser_p = (uint8_t*)&microsuser;
    uint8_t* microskernel_p = (uint8_t*)&microskernel;
    uint8_t* bytesread_p = (uint8_t*)&bytesread;
    uint8_t* byteswritten_p = (uint8_t*)&byteswritten;
    uint8_t* sleepytime16_p = (uint8_t*)&sleepytime16;

    int statfile = open( filename, O_APPEND|O_CREAT|O_WRONLY, S_IRUSR|S_IWUSR );
    if( statfile < 0 ) {
        print_error( "Could not open statistics file" );
        exit( -1 );
    }

    time_t now;
    struct tm* now_tm;
    time( &now );
    now_tm = gmtime( &now );
    snprintf( buf2, 1024, "%04i-%02i-%02i %02i:%02i:%02i\n", 1900+now_tm->tm_year, 1+now_tm->tm_mon, now_tm->tm_mday, now_tm->tm_hour, now_tm->tm_min, now_tm->tm_sec );

    // Take first measures
    if( getrusage( RUSAGE_SELF, &ru1 ) ) { // Just to check that it works
        print_error( "getrusage failed" );
        exit( -1 );
    }
    rx1 = 0; // By definition (since we only start measuring when the process begins)
    tx1 = 0;

    if( gettimeofday( &tv1, NULL ) < 0 ) {
        print_error( "Could not get the time of day" );
        exit( -1 );
    }
    tv1.tv_usec -= 1000; // Offset for correct first timing

    // Mark statistics log
    if( write( statfile, buf2, strlen( buf2 ) ) < 0 ) {
        print_error( "Could not write to statistics file" );
        exit( -1 );
    }

    // Mark statistics version
    write( statfile, "1\n", 2 );

    // Prepare buf2 for more efficient usage down below
    buf2[0] = 'a';
    buf2[9] = 'b';
    buf2[18] = 'c';
    buf2[27] = 'd';
    buf2[36] = 'e';
    buf2[41] = '\n';
    buf2[42] = 0;

    // Start measurements
    while( !end_loop ) {
        // Get measurements
        getrusage( RUSAGE_SELF, &ru2 );
        microsuser = (uint32_t)( ru2.ru_utime.tv_sec * 1000000 + ru2.ru_utime.tv_usec );
        microskernel = (uint32_t)( ru2.ru_stime.tv_sec * 1000000 + ru2.ru_stime.tv_usec );

        rx2 = Channel::totalBytesRead();
        bytesread = rx2 - rx1;
        rx1 = rx2;

        tx2 = Channel::totalBytesSent();
        byteswritten = tx2 - tx1;
        tx1 = tx2;

        // Transform data to a fast but robust format to be saved
        microsuser = htobe32(microsuser);
        microskernel = htobe32(microskernel);
        bytesread = htobe32(bytesread);
        byteswritten = htobe32(byteswritten);
        sleepytime16 = htobe16((uint16_t)sleepytime);

        buf2[1] = ( microsuser_p[0] >> 4 ) | 64;
        buf2[2] = ( microsuser_p[0] & 15 ) | 64;
        buf2[3] = ( microsuser_p[1] >> 4 ) | 64;
        buf2[4] = ( microsuser_p[1] & 15 ) | 64;
        buf2[5] = ( microsuser_p[2] >> 4 ) | 64;
        buf2[6] = ( microsuser_p[2] & 15 ) | 64;
        buf2[7] = ( microsuser_p[3] >> 4 ) | 64;
        buf2[8] = ( microsuser_p[3] & 15 ) | 64;

        buf2[10] = ( microskernel_p[0] >> 4 ) | 64;
        buf2[11] = ( microskernel_p[0] & 15 ) | 64;
        buf2[12] = ( microskernel_p[1] >> 4 ) | 64;
        buf2[13] = ( microskernel_p[1] & 15 ) | 64;
        buf2[14] = ( microskernel_p[2] >> 4 ) | 64;
        buf2[15] = ( microskernel_p[2] & 15 ) | 64;
        buf2[16] = ( microskernel_p[3] >> 4 ) | 64;
        buf2[17] = ( microskernel_p[3] & 15 ) | 64;

        buf2[19] = ( bytesread_p[0] >> 4 ) | 64;
        buf2[20] = ( bytesread_p[0] & 15 ) | 64;
        buf2[21] = ( bytesread_p[1] >> 4 ) | 64;
        buf2[22] = ( bytesread_p[1] & 15 ) | 64;
        buf2[23] = ( bytesread_p[2] >> 4 ) | 64;
        buf2[24] = ( bytesread_p[2] & 15 ) | 64;
        buf2[25] = ( bytesread_p[3] >> 4 ) | 64;
        buf2[26] = ( bytesread_p[3] & 15 ) | 64;

        buf2[28] = ( byteswritten_p[0] >> 4 ) | 64;
        buf2[29] = ( byteswritten_p[0] & 15 ) | 64;
        buf2[30] = ( byteswritten_p[1] >> 4 ) | 64;
        buf2[31] = ( byteswritten_p[1] & 15 ) | 64;
        buf2[32] = ( byteswritten_p[2] >> 4 ) | 64;
        buf2[33] = ( byteswritten_p[2] & 15 ) | 64;
        buf2[34] = ( byteswritten_p[3] >> 4 ) | 64;
        buf2[35] = ( byteswritten_p[3] & 15 ) | 64;

        buf2[37] = ( sleepytime16_p[0] >> 4 ) | 64;
        buf2[38] = ( sleepytime16_p[0] & 15 ) | 64;
        buf2[39] = ( sleepytime16_p[1] >> 4 ) | 64;
        buf2[40] = ( sleepytime16_p[1] & 15 ) | 64;

        // Write line of statistics
        write( statfile, buf2, 42 );
        
        // Sleep until next timeslot (100 Hz)
        tv1.tv_usec += 10000;
        if( tv1.tv_usec > 1000000 ) {
            tv1.tv_usec -= 1000000;
            tv1.tv_sec++;
        }
        gettimeofday( &tv2, NULL );
        sleepytime = 10000 - ((tv2.tv_sec - tv1.tv_sec) * 1000000 + (tv2.tv_usec - tv1.tv_usec));
        if( sleepytime && sleepytime < 20000 ) // Guard against no sleep or too much sleep (too much being a negative value :/)
            usleep( sleepytime );
    }

    close( statfile );

    return NULL;
}

int main (int argc, char** argv) {
    // nodht TODO
    // old: daemon, progress, http
    //
    // TODO: Add option to seed/leech fake data of given size, with given multiplier  -> 2TB with multiplier can use 250M data times 8K using a bitmap of 250M to keep track of which parts are received
    
    // The long options that are allowed. See below (or run with -?) for the complete usage.
    static struct option long_options[] =
    {
        { "seed",       no_argument,        0, 's' },
        { "leech",      no_argument,        0, 'g' },
        { "get",        no_argument,        0, 'g' },
        { "help",       no_argument,        0, '?' },

        { "hash",       required_argument,  0, 'h' },
        { "file",       required_argument,  0, 'f' },
        { "listen",     required_argument,  0, 'l' },
        { "tracker",    required_argument,  0, 't' },
        { "maxtime",    required_argument,  0, 'm' },
        { "multiply",   required_argument,  0, '*' },
        { "size",       required_argument,  0, '+' },
        { "many",       required_argument,  0, '#' },
        { "offset",     required_argument,  0, 'o' },
        { "stat",       required_argument,  0, '@' },
        { "debug",      no_argument,        0, 'd' },
        { "baseoffset", required_argument,  0, '$' },

        {0, 0, 0, 0}
    };

    // Root hash to be leeched
    Sha1Hash root_hash;
    // Filename to use
    char* filename = 0;
    // Address to listen on
    Address bind_address;
    // Tracker to use
    Address tracker;
    // Operation mode (1: leeching, 2: seeding)
    int mode = 0;
    // Time to wait (0 for infinite)
    int wait_time = 0;
    // Offset for each next seed
    int offset = 1;
    // Number of seeds to start
    int seedcount = 1;
    // Name of the statistics file
    char* statistics = 0;
    // Array with the addresses of the former two
    char** statargs[] = { &statistics };
    // Number of repetitions in the file to seed
    int multiply = 1;
    // Known size of the file to receive in blocks (0: auto)
    int knownsize = 0;
    // Offset for first seed
    int baseoffset = 0;
    
    bool debug = false;
    
    // Initialize the library
    LibraryInit();
    
    // Parse options
    int c;
    char* ret;
    while( -1 != ( c = getopt_long( argc, argv, ":h:f:l:t:m:sgd", long_options, 0 ) ) ) {
        switch(c) {
            case 'h' :
                if( strlen( optarg ) != 40 )
                    quit( "SHA1 hash must be 40 hex symbols\n" );
                root_hash = Sha1Hash( true, optarg );
                if( root_hash == Sha1Hash::ZERO )
                    quit( "SHA1 hash must be 40 hex symbols\n" );
                break;
            case 'f' :
                filename = strdup( optarg );
                break;
            case 'l' :
                bind_address = Address( optarg );
                if( bind_address == Address() )
                    quit( "address must be hostname:port, ip:port or just port\n" );
                break;
            case 't' :
                tracker = Address( optarg );
                if( tracker == Address() )
                    quit( "address must be hostname:port, ip:port or just port\n" );
                break;
            case 's' :
                if( mode )
                    quit( "concurrent seeding and leeching not supported\n" );
                mode = 2;
                break;
            case 'g' :
                if( mode )
                    quit( "concurrent seeding and leeching not supported\n" );
                mode = 1;
                break;
            case 'm' :
                wait_time = readPositiveIntArg();
                break;
            case '*' : // --multiply
                multiply = readPositiveIntArg();
                break;
            case '+' : // --size
                knownsize = readPositiveIntArg();
                break;
            case '#' : // --many
                seedcount = readPositiveIntArg();
                break;
            case 'o' : // --offset
                offset = readPositiveIntArg();
                break;
            case '@' : // --stat
                statistics = strdup( optarg );
                break;
            case '$' : // --baseoffset
                baseoffset = readPositiveIntArg();
                break;
            case 'd' :
                debug = true;
                break;
            case '?' :
                printf( "libswift command line test program\n" );
                printf( "Usage: %s action [options]\n", argv[0] );
                printf( "\n" );
                printf( "Possible actions:\n" );
                printf( "-s, --seed           We're seeding\n" );
                printf( "-g, --leech          We're leeching\n" );
                printf( "-?, --help           Show this help\n" );
                printf( "Possible options:\n" );
                printf( "-h, --hash           Roothash of the file we're leeching (required when leeching, forbidden when only seeding)\n" );
                printf( "-f, --file           Filename to use (file to seed when seeding (required), file to save to when leeching (defaults to hash))\n" );
                printf( "-l, --listen         [(ip|hostname):]port on which to listen when seeding (defaults to random port)\n" );
                printf( "-t, --tracker        [(ip|hostname):]port where to find a tracker (defaults to none)\n" );
                printf( "-m, --maxtime        Time in seconds to remain active, ie. maximum time to leech or seed (positive integer, defaults to infinite)\n" );
                printf( "--multiply           Fake seeding more data by virtually multiplying the file a given number of times; also usable for leeching when it is known that such data will arrive (positive integer, power of 2, defaults to 1)\n" );
                printf( "--size               Size of data to seed/leech in blocks (1 block == 1 hash); required when specifying --multiply; this is used only to initialize the hash tree to the correct size (positive integer, power of 2, defaults to auto)\n" );
                printf( "--many               Instead of seeding 1 file, seed the given file a given number of times, each next seed starting at +offset from the previous (positive integer, defaults to 1)\n" );
                printf( "--offset             The offset for each next seed (positive integer, defaults to 1)\n" );
                printf( "--stat               Write statistics to the specified file (filename, defaults to no statistics)\n" );
                printf( "--baseoffset         Offset into the file for the first seed (positive integer, defaults to 0)\n" );
                printf( "-d, --debug          Output debug info to standard out (defaults to no)\n" );
                return 0;
        }
    }

    if( debug )
        Channel::debug_file = stdout;

    // Check mode
    if( !mode ) {
        quit( "no action found, specify --seed or --leech, or specify --help for more information\n" );
    }
    else if( mode == 1 ) {
        // Check leeching settings
        if( root_hash == Sha1Hash::ZERO )
            quit( "--hash required when leeching\n" );
    }
    else if( mode == 2 ) {
        // Check seeding settings
        if( !filename )
            quit( "--file required when seeding\n" );
    }

    // Check multiply and size arguments
    if( multiply > 1 && !knownsize )
        quit( "--size required when leeching with --multiply\n" );
    if( multiply > 1 ) {
        int multiplycheck = multiply;
        while( !( multiplycheck & 1 ) )
            multiplycheck >>= 1;
        if( multiplycheck > 1 )
            quit( "--multiply requires a power of 2 as its argument\n" );
    }
    if( knownsize ) {
        int knownsizecheck = knownsize;
        if( knownsize < 2 )
            quit( "--size requires a power of 2 and at least 2 as its argument\n" );
        while( !( knownsizecheck & 1 ) )
            knownsizecheck >>= 1;
        if( knownsizecheck > 1 )
            quit( "--size requires a power of 2 and at least 2 as its argument\n" );
    }

    // Bind to port
    int socket;
    if( bind_address != Address() )
        socket = Listen( bind_address );
    else {
        // Random port. Try 10 times just in case
        for( int i = 0; i < 10; i++ ) {
            bind_address = Address( (uint32_t)INADDR_ANY, 0 );
            if( ( socket = Listen( bind_address ) ) <= 0 )
                continue;
            break;
        }
    }
    if( !socket ) {
        quit( "can't listen to %s\nno socket was created\n", bind_address.str() );
    }
    else if( socket < 0 ) {
        quit( "can't listen to %s\n%s\n", bind_address.str(), strerror( errno ) );
    }
    
    // Set tracker if needed
    if( tracker != Address() )
        SetTracker( tracker );

    // Set default filename if needed
    if( root_hash != Sha1Hash::ZERO && !filename )
        filename = strdup( root_hash.hex().c_str() );

    // Register our signal handler to catch SIGINT
    static struct sigaction signal_action;
    bzero( &signal_action, sizeof( struct sigaction ) );
    signal_action.sa_flags = SA_RESETHAND;
    signal_action.sa_handler = catch_signal;
    if( sigaction( SIGINT, &signal_action, 0 ) )
        quit( "could not register signal handler for SIGINT: %s\n", strerror( errno ) );

    // If needed, fire off statistics thread
    pthread_t child;
    if( statistics ) {
        pthread_attr_t attr;
        struct sched_param sp;
        bzero( &sp, sizeof( struct sched_param ) );
        sp.sched_priority = 1;
        if( errno = pthread_attr_init( &attr ) ) {
            print_error( "Could not create statistics thread attributes" );
            exit(-1);
        }
        if( errno = pthread_attr_setdetachstate( &attr, PTHREAD_CREATE_JOINABLE ) ) {
            print_error( "Could not set statistics thread detach state" );
            exit(-1);
        }
        if( errno = pthread_attr_setschedpolicy( &attr, SCHED_FIFO ) ) {
            print_error( "Could not set statistics thread scheduling policy" );
            exit(-1);
        }
        if( pthread_attr_setschedparam( &attr, &sp ) ) {
            print_error( "Could not set statistics thread scheduling priority" );
            exit(-1);
        }
        if( errno = pthread_create( &child, &attr, statisticsThread, statargs ) ) {
            print_error( "Could not create statistics thread" );
            exit(-1);
        }
        pthread_attr_destroy( &attr );
    }

    // Open file (sets up seed or leech)
    FileTransfer* files[seedcount];
    bzero( files, seedcount * sizeof(FileTransfer*) );
    int i;
    for( i = 0; i < seedcount; i++ ) {
        // Essentially this is just swift::Open, but allowing different storages
        if( multiply == 1 ) {
            if( mode == 2 && ( i > 0 || baseoffset > 0 ) )
                files[i] = new FileTransfer( new FileOffsetDataStorage( filename, baseoffset+offset*i ), root_hash );
            else
                files[i] = new FileTransfer( filename, root_hash );
        }
        else {
            char buf[1024];
            snprintf( buf, 1024, "%s.+%03d.mhash", filename, i );
            RepeatingHashStorage* rhs = new RepeatingHashStorage( buf, multiply );
            if( !rhs->setHashCount( knownsize ) ) {
                delete rhs;
                quit( "Can't set the hash count for file %i to %i\n", i, knownsize );
            }
            FileOffsetDataStorage* fods = new FileOffsetDataStorage( filename, offset*i, multiply );
            if( !fods->setSize( knownsize * 1024 ) ) {
                delete rhs;
                delete fods;
                quit( "Can't set the file size for file %i to %i\n", i, knownsize * 1024 );
            }
            files[i] = new FileTransfer( fods, root_hash, rhs );
        }
        if( !files[i] || !files[i]->file().data_storage() ) {
            for( i = 0; i < seedcount; i++ ) {
                if( files[i] )
                    Close( files[i] );
            }
            quit( "cannot open file %s (seed %i)\n", filename, i );
        }
        if( Channel::Tracker() != Address() )
            new Channel(files[i]);
        Sha1Hash file_hash = RootMerkleHash( files[i] );
        if( !i && root_hash != Sha1Hash::ZERO && !( file_hash == root_hash ) ) { // Only check for first seed
            Close( files[0] );
            quit( "seeding file with root hash %s, but hash %s was specified: mismatch\n", file_hash.hex().c_str(), root_hash.hex().c_str() );
        }
        if( mode == 2 )
            printf( "Root hash of seed %i: %s\n", i, file_hash.hex().c_str() );
        root_hash = Sha1Hash::ZERO;
    }
    // From here on root_hash is no longer valid

    // Time at start
    tint end_time = NOW + (wait_time * TINT_SEC);

    // Working loop
    if( mode == 1 ) {
        // Leeching
        while( ( files[0] >= 0 && !IsComplete( files[0] ) ) && ( !wait_time || end_time > NOW ) && !end_loop )
            swift::Loop( TINT_SEC );
    }
    else if( mode == 2 ) {
        // Seeding
        while( ( !wait_time || end_time > NOW ) && !end_loop )
            swift::Loop( TINT_SEC );
    }
    end_loop = true;
   
    // Cleanup
    for( i = 0; i < seedcount; i++ ) {
        if( files[i] )
            Close( files[i] );
    }
    if( Channel::debug_file && Channel::debug_file != stdout )
        fclose( Channel::debug_file );
    
    // Shutdown library
    swift::Shutdown();
    
    if( statistics )
        pthread_join( child, NULL );

    return 0;
}

