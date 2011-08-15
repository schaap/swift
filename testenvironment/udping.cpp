#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>

int main( ) {
    int sock = socket( AF_INET, SOCK_DGRAM, 0 );

    char buf[1];
    struct sockaddr_in sa;
    sa.sin_family = AF_INET;
    sa.sin_port = htons(20000);
    inet_pton( AF_INET, "127.0.0.1", &sa.sin_addr );
    int ret = sendto( sock, buf, 0, 0, (struct sockaddr*) &sa, sizeof sa );
    switch( ret ) {
        case 0 :
            printf( "Succes!\n" );
            break;
        case -1 :
            switch( errno ) {
                case EBADF :
                    printf( "EBADF\n" );
                    break;
                case ECONNRESET :
                    printf( "ECONNRESET\n" );
                    break;
                
            }
            break;
        default :
            if( ret < 0 )
                printf( "ret = %i, unexpected\n", ret );
            else
                printf( "sent %i bytes, unexpected\n", ret );
    }

    return 0;
}
