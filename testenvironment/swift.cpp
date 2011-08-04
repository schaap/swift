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


using namespace swift;


#define quit(...) {fprintf(stderr,__VA_ARGS__); exit(1); }


// Global variable to correctly stop execution on SIGINT
volatile bool end_loop = false;
// And a simple signal catcher to catch SIGINT
void catch_signal( int signum ) {
    if( signum == SIGINT )
        end_loop = true;
}


int main (int argc, char** argv) {
    // nodht TODO
    // old: daemon, debug, progress, http
    
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
                errno = 0;
                wait_time = strtol( optarg, &ret, 10 );
                if( errno )
                    quit( "invalid integer: %s\n", strerror( errno ) );
                if( *ret != 0 )
                    quit( "integer contains invalid characters: %c\n", (int)(*ret) );
                if( wait_time <= 0 )
                    quit( "wait time must be larger than 0\n" );
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
                return 0;
        }
    }

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

    // Open file
    int file = -1;
    if( filename ) {
        file = Open( filename, root_hash );
        if( file <= 0 )
            quit( "cannot open file %s\n", filename );
        Sha1Hash file_hash = RootMerkleHash( file );
        if( root_hash != Sha1Hash::ZERO && !( file_hash == root_hash ) )
            quit( "seeding file with root hash %s, but hash %s was specified: mismatch\n", file_hash.hex().c_str(), root_hash.hex().c_str() );
        printf( "Root hash: %s\n", file_hash.hex().c_str() );
        root_hash = file_hash;
    }

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
        while( ( file >= 0 && !IsComplete( file ) ) && ( !wait_time || end_time > NOW ) && !end_loop )
            swift::Loop( TINT_SEC );
    }
    else if( mode == 2 ) {
        // Seeding
        while( ( !wait_time || end_time > NOW ) && !end_loop )
            swift::Loop( TINT_SEC );
    }
   
    // Cleanup
    if( file != -1 )
        Close( file );
    if( Channel::debug_file )
        fclose( Channel::debug_file );
    
    // Shutdown library
    swift::Shutdown();
    

    return 0;
}

