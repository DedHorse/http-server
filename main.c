#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>

int getRequest( int conn, char* req_buf )
{
	char filename_buf[99];
	memset( filename_buf, 0, sizeof( filename_buf ) );
	int i = sizeof( "GET" ) + 1;
	int j = 0;
	while( req_buf[i] != ' ' )
	{
		filename_buf[j] = req_buf[i];
		++i;
		++j;
	}

	char http_ver_buf[99];
	memset( http_ver_buf, 0, sizeof(http_ver_buf) );
	j = 0;
	while( req_buf[i] != '\r' && req_buf[i+1] != '\n' )
	{
		http_ver_buf[j] = req_buf[i];
		++i;
		++j;
	}

	printf( "[*] File requested: %s\n", filename_buf );
	printf( "[*] HTTP version: %s\n", http_ver_buf );
	
	char file_buf[99];

	int fd = open( filename_buf, O_RDONLY );
	if( fd < 0 )
	{
		//return 404
		printf("404\n");
		return 0;
	}

	if( read( fd, file_buf, sizeof( file_buf ) ) < 0 )
	{
		perror( "[-] Failed to read file" );
		exit( EXIT_FAILURE );
	}

	close( fd );

	printf( "[*] Requested content: %s\n", file_buf );

	send( conn, file_buf, sizeof(file_buf), 0 );	

	return 0;
}

int handleClient( int conn )
{
	char buf[99];
	memset( buf, 0, sizeof( buf ) );

	recv( conn, buf, sizeof( buf ), 0 );

	printf( buf );

	char method_buf[6];
	memset( method_buf, 0, sizeof( method_buf ) );
	int i = 0;
	while( buf[i] != ' ' )
	{
		method_buf[i] = buf[i];
		++i;
	}

	printf( "[*] Request method: %s\n", method_buf );	

	if( strcmp( method_buf, "GET" ) == 0 )
		getRequest( conn, buf );

	return 0;
}

int main( int arcg, char** argv )
{
	char* addr = "127.0.0.1";
	uint16_t port = 80;

	int sock = socket( PF_INET, SOCK_STREAM, 0 );
	if( sock < 0 )
	{
		perror( "[-] Couldn't create socket!\n" );
		exit( EXIT_FAILURE );
	}

	const int enable_opt = 1;

	if( setsockopt( sock, SOL_SOCKET, SO_REUSEADDR, &enable_opt, sizeof(int) ) < 0 )
	{
		perror( "[-] Couldn't set socket options.\n" );
		exit( EXIT_FAILURE );
	}

	struct sockaddr_in name;
	name.sin_family 	= 	AF_INET;
	name.sin_port 		= 	htons( port );
	name.sin_addr.s_addr 	=	inet_addr(addr);

	int name_size = sizeof( name );

	if( bind( sock, (struct sockaddr *) &name, name_size ) < 0 )
	{
		perror( "[-] Couldn't bind socket\n" );
		exit( EXIT_FAILURE );
	}

	struct sockaddr_in client;
	int client_size = sizeof(client);

	while( 1 )
	{
		listen( sock, 10 );
		int conn = accept( (int) sock, (struct sockadrr *) &client, &client_size );
		if(conn < 0)
		{
			printf( "[-] Failed to accept connection.\n" );
			perror( EXIT_FAILURE );
		}

		char client_ip_buf[INET_ADDRSTRLEN];
		inet_ntop( AF_INET, &client.sin_addr, client_ip_buf, sizeof(client_ip_buf) );
		printf( "[*] Connection from %s:%d\n", client_ip_buf, ntohs( client.sin_port ) );
		
		handleClient( conn );

		break;
	}

	close( socket );
	printf( "[*] Exiting.\n" );
	return 0;
}
