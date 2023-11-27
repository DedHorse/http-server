#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <string.h>
#include "http-09.h"

int isFile( char* full_path );

int sendResponseV09( char* webroot, char* resource, int conn )
{
	char full_path[ strlen( webroot ) + strlen( resource ) ];
	memset( full_path, '\0', strlen( full_path ) );
	strcat( full_path, webroot );
	strcat( full_path, resource );

	printf( "%s\n", full_path );

	int fd = open( full_path, O_RDONLY );

	if( fd <= 0 || !isFile( full_path ) )
	{
		char* file_not_found = "<html>\nERROR no such file\n</html>";
		send( conn, file_not_found, strlen( file_not_found ), 0 );
		printf( "[-] File not found.\n" );
		return 0;
	}

	FILE* fp = fdopen( fd, "r" );
	fseek( fp, 0L, SEEK_END );
	int file_size = ftell( fp );
	rewind( fp );

	char* file_content = (char*) malloc( file_size * sizeof(char) );
	
	if( fread( file_content, sizeof(char), file_size, fp ) != file_size )
	{
		char* failed_to_read_file = "<html>\nERROR failed to open file.\n</html>";
		send( conn, failed_to_read_file, strlen( failed_to_read_file ), 0 );
		free( file_content );
		fclose( fp );
		return 0;
	}

	fclose( fp );

	send( conn, file_content, file_size, 0 );

	free( file_content );

	return 1;
}

int parseRequestedResourceV09( char* buf, int buf_len, char* resource )
{
	int i = 0;
	while( buf[i] != ' ' )
	{
		++i;
		if( i >= buf_len )
			return 0;
	}

	++i; // hack to skip the space
	int j = 0;

	while( buf[i] != ' ' )
	{
		resource[j] = buf[i];
		++i;
		++j;
		if( i>= buf_len )
			return 0;
	}

	return 1;
}

void http09Handler( char* buf, int buf_len, int conn, char* webroot )
{
	char* resource = (char*) malloc( 1024 * sizeof(char) );
	memset( resource, '\0', sizeof(resource) );

	if( !parseRequestedResourceV09( buf, buf_len, resource ) )
	{
		char* malformed_req_response = "<html>\nERROR malformed request\n</html>";
		send( conn, malformed_req_response, strlen( malformed_req_response ), 0 );
		printf( "[-] Client sent malformed request.\n" );
		return;
	}

	printf( "[*] Client requested %s\n", resource );

	if( !sendResponseV09( webroot, resource, conn ) )
		printf( "[-] Failed to send response to client.\n" );
	else
		printf( "[+] Response sent successfuly.\n" );
	free( resource );
}
