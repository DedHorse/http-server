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

void sendResource( char* webroot, char* resource, int conn )
{
	char full_path[ strlen(webroot) + strlen(resource) ];
	strcpy( full_path, webroot );
	strcat( full_path, resource );

	int fd = open( full_path, O_RDONLY );

	if( fd <= 0  || !isFile( full_path ) )
	{
		char* file_not_found = "<html>\nNo such file\n</html>";
		send( conn, file_not_found, strlen( file_not_found ), 0 );
		close( conn );
		printf( "(Not found)\n" );
		return;
	}
	
	FILE* fp = fdopen( fd, "r" );
	fseek( fp, 0L, SEEK_END );
	int file_size = ftell( fp );
	rewind( fp );

	char* file_content = malloc( file_size ); 

	if( fread( file_content, sizeof( *file_content ), file_size, fp ) != file_size )
	{
		perror( "[-] Failed to read file" );
		fclose( fp );
		free( file_content );
		return;
	}

	fclose( fp );

	send( conn, file_content, file_size, 0 );

	printf( "(Found)\n" );

	free( file_content );
}

// 0 = success
// 1 = error

int parseRequestedResource( char* buf, int buf_len, char* resource )
{
	int i = 0;
	while( buf[i] != ' ' )
	{
		++i;
		if( i >= buf_len )
			return 1;
	}

	++i;		// hack to skip the space
	int j = 0;

	while( buf[i] != '\n' )
	{
		if( buf[i] != '\r' )
			resource[j] = buf[i];
		++i;
		++j;
		if( i >= buf_len )
			return 1;
	}

	return 0;
}

void *handleConnection( void *args )
{
	handle_conn_args args_struct = *(handle_conn_args *) args;

	int conn = args_struct.conn;
	char* webroot = args_struct.webroot;

	// int conn, char* webroot
	char buf[1024];
	memset( buf, 0, sizeof( buf ) );
	int buf_len = recv( conn, buf, sizeof( buf ), 0 );

	char resource[1024];
	memset( resource, 0, sizeof( resource ) );
	int success = parseRequestedResource( buf, buf_len, resource );

	if(success == 1)
	{
		char* malformed_req_response = "<html>\nERROR Malformed Request\n</html>";
		send( conn, malformed_req_response, strlen( malformed_req_response ), 0 );
		printf( "[-] Client sent malformed request.\n" );
		close( conn );
		pthread_exit( NULL );
	}

	printf( "[*] Client requested %s ", resource ); // status gets printed by send resource

	sendResource( webroot, resource, conn );
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

	signal(SIGINT, exit_prog);

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
	// listen for connections
		listen( sock, 10 );
	// handle connections
		int conn = accept( sock, (struct sockaddr*) &client, (socklen_t * restrict) &client_size );
		if( conn < 0 )
		{
			perror( "[-] Failed to accept connection.\n" );
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
