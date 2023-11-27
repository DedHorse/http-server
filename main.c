#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include "http-09.h"

int CTRL_C_PRESSED = 0;

typedef struct _handle_conn_args
{
	int conn;
	char* webroot;
} handle_conn_args;

int isFile( const char* path )
{
	struct stat path_s;

	return !stat( path, &path_s ) && S_ISREG( path_s.st_mode );
}

int getReqHttpVer( char* buf, int buf_len )
{
	char parsed_version[4];
	parsed_version[3] = '\0';
	for( int i = 4; i < buf_len; ++i )
	{
		if( buf[i-4] == 'H' &&
		    buf[i-3] == 'T' &&
		    buf[i-2] == 'T' &&
		    buf[i-1] == 'P' &&
		    buf[i]   == '/' )
		{
			int j = i+1; // skip the slash
			int ver_index = 0;
			while( buf[j] != '\r' && buf[j] != '\n' )
			{
				if( ver_index > 2 )
					return -1;
				parsed_version[ver_index] = buf[j];
				++ver_index;
				++j;
			}
			break;
		}
	}
	if( strcmp( parsed_version, "0.9" ) == 0 )
		return 0;
       return -1;	
}

// change this to only detect http version and let spec specific functions handle the rest

void *handleConnection( void *args )
{
	handle_conn_args args_struct = *(handle_conn_args *) args;

	int conn = args_struct.conn;
	char* webroot = args_struct.webroot;

	char buf[1024];
	memset( buf, 0, sizeof( buf ) );
	int buf_len = recv( conn, buf, sizeof( buf ), 0 );

	int ver = getReqHttpVer( buf, buf_len );

	switch( ver )
	{
		case 0:
			http09Handler( buf, buf_len, conn, webroot );
			break;
		default:
			printf( "[-] Couldn't get HTTP version.\n" );
			break;
	}

	close( conn );

	pthread_exit( NULL );
}

int createSocket( char* host, int port )
{
	struct sockaddr_in name;
	int sock = socket( PF_INET, SOCK_STREAM, 0 );
	if( sock < 0 )
		return -1;
	
	name.sin_family = AF_INET;
	name.sin_port = htons( port );
	name.sin_addr.s_addr = inet_addr( host );

	const int enable = 1;
	if( setsockopt( sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof( enable ) ) < 0 )
	{
		perror( "[-] Failed to set sock options" );
		exit( EXIT_FAILURE );
	}

	if( bind( sock, (struct sockaddr*) &name, sizeof( name ) ) < 0 )
		return -2;

	return sock;
}

void exit_prog( int sig_num )
{
	CTRL_C_PRESSED = 1;
}

int main(void)
{
	printf( "[*] Starting.\n" );

	signal( SIGINT, exit_prog );

	char* webroot = ".";
	// setup socket
	char* host = "127.0.0.1";
	int port = 8080;	
	int sock = createSocket( host, port );

	if( sock == -1 )
	{
		perror( "[-] Failed to create socket.\n" );
		exit( EXIT_FAILURE );
	}
	if( sock == -2 )
	{
		perror( "[-] Failed to bind socket.\n" );
		exit( EXIT_FAILURE );
	}

	printf( "[+] Socket created successfuly.\n" );

	struct sockaddr_in client;
	int client_size = sizeof( client );

	printf( "[+] Listnening for connections.\n" );
	
	// TODO figure out how to handle this all with nonblocking I/O and threads.

	pthread_t thread_id;

	while( !CTRL_C_PRESSED )
	{
		listen( sock, 10 );
		int conn = accept( sock, (struct sockaddr*) &client, (socklen_t * restrict) &client_size );
		if( conn < 0 )
		{
			printf( "[-] Failed to accept connection.\n" );
			continue;
		}
	
		char client_ip[INET_ADDRSTRLEN];
		inet_ntop( AF_INET, &client.sin_addr, client_ip, sizeof( client_ip ) );
		printf( "[*] Connection from %s on port %d.\n", client_ip, ntohs( client.sin_port ) );

		handle_conn_args args;
		args.conn = conn;
		args.webroot = webroot;

		pthread_create( &thread_id, NULL, handleConnection, (void *)&args );

	// back to loop, if ctrl+c pressed break
	}
	// clean up
	
	close( sock );
	printf( "[*] Exiting.\n" );

	return 0;
}
