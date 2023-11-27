#ifndef HTTPZERONINEHEADER

void http09Handler( char* buf, int buf_len, int conn, char* webroot );

int parseRequestedResourceV09( char* buf, int buf_len, char* resource );

int sendResponseV09( char* webroot, char* resource, int conn );

#endif
#define HTTPZERONINEHEADER
