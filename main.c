#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>

int CTRL_C_PRESSED = 0;

void sendResource( char* webroot, char* resource, int conn )
{
	char full_path[ strlen(webroot) + strlen(resource) ];
	strcpy( full_path, webroot );
	strcat( full_path, resource );

	//printf("%s\n", full_path);

	int fd = open( full_path, O_RDONLY );

	if( fd <= 0 )
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

void parseRequestedResource( char* buf, char* resource )
{
	int i = 0;
	while( buf[i] != ' ' )
		++i;

	++i;		// hack to skip the space
	int j = 0;

	while( buf[i] != '\n' )
	{
		if( buf[i] != '\r' )
			resource[j] = buf[i];
		++i;
		++j;
	}
}

void handleConnection( int conn, char* webroot )
{
	char buf[1024];
	memset( buf, 0, sizeof( buf ) );
	recv( conn, buf, sizeof( buf ), 0 );

	char resource[1024];
	memset( resource, 0, sizeof( resource ) );
	parseRequestedResource( buf, resource );

	printf( "[*] Client requested %s ", resource ); // status gets printed by send resource

	sendResource( webroot, resource, conn );
	close( conn );
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
	int port = 80;	
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
	
	while( !CTRL_C_PRESSED )
	{
	// listen for connections
		listen( sock, 10 );
	// handle connections
		int conn = accept( sock, (struct sockaddr*) &client, &client_size );
		if( conn < 0 )
		{
			perror( "[-] Failed to accept connection.\n" );
			continue;
		}
	
		char client_ip[INET_ADDRSTRLEN];
		inet_ntop( AF_INET, &client.sin_addr, client_ip, sizeof( client_ip ) );
		printf( "[*] Connection from %s on port %d.\n", client_ip, ntohs( client.sin_port ) );

		handleConnection( conn, webroot );

	// back to loop, if ctrl+c pressed break
	}
	// clean up
	
	close( socket );
	printf( "[*] Exiting.\n" );

	return 0;
}
