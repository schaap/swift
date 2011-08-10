#define quit( x ) { perror(x); exit(-1); }

int main( int argc, char** argv ) {
    if( argc < 3 )
        printf( "Usage: %s in-file out-file-basename\n", argv[0] );

    int input = open( argv[1], O_RDONLY );
    if( input == -1 )
        quit( argv[1] );
    int output = -1;
    char buf[1024];
    char buf2[1024];
    int n, i, p;
    int state = 0;
    struct tm header;
    int version = 0;

    n = read( input, buf, 1024 );
    if( n < 0 )
        quit( "reading input" );

    p = 0;
    for( i = 0; i < n; i++ ) {
        // Read next byte
        buf2[p++] = buf[i];
        if( buf[i] == '\n' )
            buf2[p-1] = 0;

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
                if( version !== 1 ) {
                    printf( "Unknown version: %i\n", version );
                    exit( -1 );
                }
                break;
            case 2 : // Reading log, expect entry or log header
                break;
            default :
                printf( "Failure! State has reached %i\n", state );
                exit( -1 );
        }
        // Line done, next line
        p = 0;
    }
}
