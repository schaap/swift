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

#include "compat.h"
#include "swift.h"
#include "fileoffsetdatastorage.h"


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

int main (int argc, char** argv) {
    // nodht TODO
    // old: daemon, debug, progress, http
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
        { "many",       required_argument,  0, '#' },
        { "offset",     required_argument,  0, 'o' },

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
    
    // Initialize the library
    LibraryInit();
    
    // Parse options
    int c;
    char* ret;
    while( -1 != ( c = getopt_long( argc, argv, ":h:f:l:t:m:sg", long_options, 0 ) ) ) {
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
                quit( "--multiply not implemented yet\n" );
            case '#' : // --many
                seedcount = readPositiveIntArg();
                break;
            case 'o' : // --offset
                offset = readPositiveIntArg();
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
                printf( "--multiply           Fake seeding more data by virtually multiplying the file a given number of times (positive integer, defaults to 1)\n" );
                printf( "--many               Instead of seeding 1 file, seed the given file a given number of times, each next seed starting at +offset from the previous (positive integer, defaults to 1)\n" );
                printf( "--offset             The offset for each next seed (positive integer, defaults to 1)\n" );
                return 0;
        }
    }

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

    // Open file (sets up seed or leech)
    FileTransfer* files[seedcount];
    bzero( files, seedcount * sizeof(FileTransfer*) );
    int i;
    for( i = 0; i < seedcount; i++ ) {
        // Essentially this is just swift::Open, but allowing different storages
        if( mode == 2 && i > 0 )
            files[i] = new FileTransfer( new FileOffsetDataStorage( filename, offset*i ), root_hash );
        else
            files[i] = new FileTransfer( filename, root_hash );
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

    // Register our signal handler to catch SIGINT
    static struct sigaction signal_action;
    bzero( &signal_action, sizeof( struct sigaction ) );
    signal_action.sa_flags = SA_RESETHAND;
    signal_action.sa_handler = catch_signal;
    if( sigaction( SIGINT, &signal_action, 0 ) )
        quit( "could not register signal handler for SIGINT: %s\n", strerror( errno ) );

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
   
    // Cleanup
    for( i = 0; i < seedcount; i++ ) {
        if( files[i] )
            Close( files[i] );
    }
    if( Channel::debug_file )
        fclose( Channel::debug_file );
    
    // Shutdown library
    swift::Shutdown();
    

    return 0;
}

