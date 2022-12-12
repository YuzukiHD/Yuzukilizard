/* J. David's webserver */
/* This is a simple webserver.
 * Created November 1999 by J. David Blackstone.
 * CSE 4344 (Network concepts), Prof. Zeigler
 * University of Texas at Arlington
 */
/* This program compiles for Sparc Solaris 2.6.
 * To compile for Linux:
 *  1) Comment out the #include <pthread.h> line.
 *  2) Comment out the line that defines the variable newthread.
 *  3) Comment out the two lines that run pthread_create().
 *  4) Uncomment the line that runs accept_request().
 *  5) Remove -lsocket from the Makefile.
 */
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>


#include "httpd.h"
#define LOG_TAG "httpserver"
#ifdef SDV
#include <cutils/log.h>
#warning "BUILD SDV HTTPD"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG ,  LOG_TAG, __VA_ARGS__)
#else
#define LOGD(...) printf(__VA_ARGS__)
#endif
typedef struct mime_typ {
	char ext[10];
	int ext_len;
	char value[50];
} mime_typ_t;
static mime_typ_t mime_tab[] = {
	{"txt", 3, "txt/plain"},
	{"ms", 2, "application/x-troff-ms"},
	{"ms", 2, "application/x-troff-ms"},
	{"mv", 2, "video/x-sgi-movie"},
	{"qt", 2, "video/quicktime"},
	{"ts", 2, "video/mp2t"},
	{"3gp", 3, "video/3gp"},
	{"asf", 3, "video/x-ms-asf"},
	{"avi", 3, "video/x-msvideo"},
	{"flv", 3, "video/x-flv"},
	{"vob", 3, "video/mpeg"},
	{"mkv", 3, "video/x-matroska"},
	{"mov", 3, "video/quicktime"},
	{"mp2", 3, "audio/mpeg"},
	{"mp3", 3, "audio/mpeg"},
	{"mp4", 3, "video/mp4"},
	{"mpe", 3, "video/mpeg"},
	{"3gp", 3, "video/mpeg"},
	{"msh", 3, "model/mesh"},
	{"mpg", 3, "video/mpeg"},
	{"dat", 3, "video/mpeg"},
	{"mpeg", 4, "video/mpeg"},
	{"mpga", 4, "audio/mpeg"},
	{"m2ts", 4, "video/mp2t"},
	{"movie", 5, "video/x-sgi-movie"},
	{"png", 3, "image/png"},
	{"bmp", 3, "image/bmp"},
	{"gif", 3, "image/gif"},
	{"jpe", 3, "image/jpeg"},
	{"jpg", 3, "image/jpeg"},
	{"jpeg", 4, "image/jpeg"},
	{"jfif", 4, "image/jpeg"},
	{"ra", 2, "audio/x-realaudio"},
	{"rmvb", 4, "appllication/vnd.rn-realmedia"},
	{"rm", 2, "appllication/vnd.rn-realmedia"},
	{"mp2", 3, "audio/mpeg"},
	{"mp3", 3, "audio/mpeg"},
	{"ram", 3, "audio/x-pn-realaudio"},
	{"wav", 3, "audio/x-wav"},
	{"wax", 3, "audio/x-ms-wax"},
	{"wma", 3, "audio/x-ms-wma"},
	{"wmv", 3, "video/x-ms-wmv"},
	{"mpga", 4, "audio/mpeg"},
	{"", 0, ""},
};
static int startFlag = 0;
static pthread_t acceptThreadId;
static httpdCallback gHttpdCallback = {NULL, NULL, NULL};

#define ISspace(x) isspace((int)(x))

#define SERVER_STRING "Server: jdbhttpd/0.1.0\r\n"

void *accept_request(void* client);
void bad_request(int);
void cat(int, FILE *);
void cannot_execute(int);
void error_die(const char *);
void execute_cgi(int, const char *, const char *, const char *);
int get_line(int, char *, int);
void headers(int, const char *);
void not_found(int);
void serve_file(int, const char *);
int startup(u_short *);
void unimplemented(int);
void requested_range_not_satisfiable(int sockfd);
void serve_whole_file(int sockfd, const char *filename, FILE *resource);
void serve_partial_file(int sockfd, const char *filename, FILE *resource, long long startPos, long long endPos, long long fileSize);
void partial_headers(int sockfd, const char *filename, long long startPos, long long endPos);
void partial_cat(int sockfd, FILE *resource, long long startPos, long long endPos, long long fileSize);
ssize_t recv_wrap(int __fd, void *__buf, size_t __n, int __flags);
ssize_t send_wrap(int __fd, __const void *__buf, size_t __n, int __flags);


ssize_t recv_wrap(int __fd, void *__buf, size_t __n, int __flags)
{
	int ret = 0;

	fd_set rfd;
	FD_ZERO(&rfd);
	FD_SET(__fd, &rfd);
	struct timeval to = {1, 0};
	int result = select(__fd+1, &rfd, NULL, NULL, &to);
	if (result > 0)
	{
		ret = recv(__fd, __buf, __n, __flags);
	}
	else
	{
		LOGD("error!, recv time out:1 sec pass\n");
		ret = -1;
	}

	return ret;
}
ssize_t send_wrap(int __fd, __const void *__buf, size_t __n, int __flags)
{
	int ret = 0;

	fd_set wfd;
	FD_ZERO(&wfd);
	FD_SET(__fd, &wfd);
	struct timeval to = {1, 0};
	int result = 1;
	result = select(__fd+1, NULL, &wfd, NULL, &to);
	if(result > 0)
	{
		ret = send(__fd, __buf, __n, __flags);
	}
	else
	{
		LOGD("error!, send time out:1 sec pass\n");
		ret = -1;
	}

	return ret;
}

/**********************************************************************/
/* A request has caused a call to accept() on the server port to
 * return.  Process the request appropriately.
 * Parameters: the socket connected to the client */
/**********************************************************************/
void *accept_request(void* client)
{
	int client_sock = *(int*)client;
	delete (int*)client;
	char buf[1024];
	int numchars;
	char method[255];
	char url[255];
	char path[512];
	size_t i, j;
	struct stat st;
	int cgi = 0;      /* becomes true if server decides this is a CGI
	                * program */
	char *query_string = NULL;

  	int snd_size;
  	socklen_t optlen;
  	int ret;

  	optlen = sizeof(snd_size);
  	ret = getsockopt(client_sock, SOL_SOCKET, SO_SNDBUF, (char*)&snd_size, &optlen);
  	if(ret < 0)
  	{
  		LOGD("getsockopt failed\n");
  	}
  	else
  	{
  		LOGD("\ndefault snd buf:%d %d\n", snd_size, optlen);

//  		snd_size = 64*1024;
  		snd_size /= 2;
  		ret = setsockopt(client_sock, SOL_SOCKET, SO_SNDBUF, (const char*)&snd_size, optlen);

  		if(ret < 0)
  		{
  			LOGD("setsockopt failed\n");
  		}
  		else
  		{
  			ret = getsockopt(client_sock, SOL_SOCKET, SO_SNDBUF, (char*)&snd_size, &optlen);
  			if(ret < 0)
  			{
  				LOGD("getsockopt failed tt\n");
  			}
  			else
  			{
  				LOGD("set snd buf:%d %d\n", snd_size, optlen);
  			}
  		}
  	}


	numchars = get_line(client_sock, buf, sizeof(buf));
	i = 0; j = 0;

	LOGD("accept_request, get_line:%s", buf); 
	while (!ISspace(buf[j]) && (i < sizeof(method) - 1))
	{
		method[i] = buf[j];
		i++; j++;
	}
	method[i] = '\0';
	LOGD("accept_request, method:%s", method);
	if (strcasecmp(method, "GET") && strcasecmp(method, "POST"))
	{
		LOGD("unimplemented");
		unimplemented(client_sock);
		return NULL;
	}

	if (strcasecmp(method, "POST") == 0)
	cgi = 1;

	i = 0;
	while (ISspace(buf[j]) && (j < sizeof(buf)))
		j++;
	while (!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < sizeof(buf)))
	{
		url[i] = buf[j];
		i++; j++;
	}
	url[i] = '\0';

	if (strcasecmp(method, "GET") == 0)
	{
		query_string = url;
		while ((*query_string != '?') && (*query_string != '\0'))
			query_string++;
		if (*query_string == '?')
		{
			cgi = 1;
			*query_string = '\0';
			query_string++;
		}
	}

	sprintf(path, "%s", url);
	if (path[strlen(path) - 1] == '/')
	strcat(path, "index.html");
	if (stat(path, &st) == -1 && strcmp(method, "POST") != 0) {
		while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
			numchars = get_line(client_sock, buf, sizeof(buf));
		not_found(client_sock);
	}
	else
	{
		if (!cgi) {
			LOGD("exe serve_file, path:%s", path);
			serve_file(client_sock, path);
		}
		else {
			LOGD("exe execute_cgi, path:%s, method:%s, query_string:%s", path, method, query_string);
			execute_cgi(client_sock, path, method, query_string);
		}
	}

	close(client_sock);
	return NULL;
}

/**********************************************************************/
/* Inform the client that a request it has made has a problem.
 * Parameters: client socket */
/**********************************************************************/
void bad_request(int client)
{
 char buf[1024];

 sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
 send_wrap(client, buf, sizeof(buf), 0);
 sprintf(buf, "Content-type: text/html\r\n");
 send_wrap(client, buf, sizeof(buf), 0);
 sprintf(buf, "\r\n");
 send_wrap(client, buf, sizeof(buf), 0);
 sprintf(buf, "<P>Your browser sent a bad request, ");
 send_wrap(client, buf, sizeof(buf), 0);
 sprintf(buf, "such as a POST without a Content-Length.\r\n");
 send_wrap(client, buf, sizeof(buf), 0);
}

/**********************************************************************/
/* Put the entire contents of a file out on a socket.  This function
 * is named after the UNIX "cat" command, because it might have been
 * easier just to do something like pipe, fork, and exec("cat").
 * Parameters: the client socket descriptor
 *             FILE pointer for the file to cat */
/**********************************************************************/
void cat(int client, FILE *resource)
{
#if 1
	char buf[1024];
	int readlen;
	int sendret;

	while((readlen = fread(buf, sizeof(char), 1024, resource)))
	{
		//printf("i = %d, readlen = %d\n", ++i, readlen);
		sendret = send_wrap(client, buf, readlen, 0);
		if(sendret < 0)
		{
			LOGD("[cat] send error! [%s]", strerror(errno));
			return;
		}
	}
	/* ����������ݻ��ߴ��� */
	if(!feof(resource))
	{
		LOGD("[cat] end before eof");
		return;
	}
#else
 char buf[1024];
 time_t startTime, curTime;
 time(&startTime);
 while (!feof(resource))
 {
  int sendlen = fread(buf, 1, sizeof(buf), resource);
  if (sendlen < 0) {
   LOGD("read error");
   break;
  }

  int ret = 0;
  while (ret < sendlen) {
   time(&curTime);
   if (curTime - startTime > 3) {
    LOGD("send nothing in 3 seconds, exit thread");
    return;
   }

   int sndret = send(client, buf+ret, sendlen-ret, 0);
   if (sndret < 0) {
    LOGD("send error:[%s]", strerror(errno));
	signal(SIGPIPE, SIG_IGN);
    usleep(100*1000);
    continue;
   }
   time(&startTime);
   ret += sndret;
  }
 }
#endif
 LOGD("send file finish");
}

/**********************************************************************/
/* Inform the client that a CGI script could not be executed.
 * Parameter: the client socket descriptor. */
/**********************************************************************/
void cannot_execute(int client)
{
 char buf[1024];

 sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
 send_wrap(client, buf, strlen(buf), 0);
 sprintf(buf, "Content-type: text/html\r\n");
 send_wrap(client, buf, strlen(buf), 0);
 sprintf(buf, "\r\n");
 send_wrap(client, buf, strlen(buf), 0);
 sprintf(buf, "<P>Error prohibited CGI execution.\r\n");
 send_wrap(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Print out an error message with perror() (for system errors; based
 * on value of errno, which indicates system call errors) and exit the
 * program indicating an error. */
/**********************************************************************/
void error_die(const char *sc)
{
 perror(sc);
 exit(1);
}

/**********************************************************************/
/* Execute a CGI script.  Will need to set environment variables as
 * appropriate.
 * Parameters: client socket descriptor
 *             path to the CGI script */
/**********************************************************************/
void execute_cgi(int client, const char *path,
                 const char *method, const char *query_string)
{
 char buf[1024];
 int cgi_output[2];
 int cgi_input[2];
 pid_t pid;
 int status;
 int i;
 char c;
 int numchars = 1;
 int content_length = -1;

 buf[0] = 'A'; buf[1] = '\0';
 if (strcasecmp(method, "GET") == 0)
  while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
   numchars = get_line(client, buf, sizeof(buf));
 else    /* POST */
 {
  numchars = get_line(client, buf, sizeof(buf));
  LOGD("buf:%s", buf);
  while ((numchars > 0) && strcmp("\n", buf))
  {
   LOGD("buf:%s", buf);
   buf[15] = '\0';
   if (strcasecmp(buf, "Content-Length:") == 0)
    content_length = atoi(&(buf[16]));
   numchars = get_line(client, buf, sizeof(buf));
  }
  if (content_length == -1) {
   bad_request(client);
   return;
  }
 }

 sprintf(buf, "HTTP/1.0 200 OK\r\n");
 send_wrap(client, buf, strlen(buf), 0);
 const char *filename = strrchr(path, '/');
 if (filename != NULL) {
  filename++;
 } else {
  filename = (char*)path;
 }
 if (gHttpdCallback.httpdStatCallback != NULL) {
  gHttpdCallback.httpdStatCallback(POST_FILE_START, (void*)filename, strlen(filename)+1, gHttpdCallback.handler);
 }
 int recvLen = 0;
 time_t recvTime;
 time(&recvTime);
 while (recvLen < content_length) {
	time_t recvTimeout;
	time(&recvTimeout);
	if (recvTimeout - recvTime > 1) {
		if (gHttpdCallback.httpdStatCallback != NULL) {
         gHttpdCallback.httpdStatCallback(POST_FILE_ERROR, (void*)filename, strlen(filename)+1, gHttpdCallback.handler);
        }
		return ;
	}
	fd_set rfd;
	FD_ZERO(&rfd);
	FD_SET(client, &rfd);
	struct timeval tv = {0, 100000};
	int sResult = select(client + 1, &rfd, NULL, NULL, &tv);
	if (sResult > 0) {
		if (FD_ISSET(client, &rfd) != 0) {
			int recvRet = recv_wrap(client, buf, 1024, 0);
			if (recvRet < 0) {
				LOGD("recv error, errno [%d]", errno);
				if (gHttpdCallback.httpdStatCallback != NULL) {
					gHttpdCallback.httpdStatCallback(POST_FILE_ERROR, (void*)filename, 
						strlen(filename)+1, gHttpdCallback.handler);
                }
				return ;
			}
//			LOGD("recv success, recvRet [%d], buf[%s]", recvRet, buf);
			if (gHttpdCallback.httpdDataCallback != NULL) {
			 gHttpdCallback.httpdDataCallback((void*)buf, recvRet, gHttpdCallback.handler);
			}
			recvLen += recvRet;
			time(&recvTime);
		}
	}
 }
 if (gHttpdCallback.httpdStatCallback != NULL) {
  gHttpdCallback.httpdStatCallback(POST_FILE_END, (void*)filename, strlen(filename)+1, gHttpdCallback.handler);
 }
#if 0
 if (pipe(cgi_output) < 0) {
  cannot_execute(client);
  return;
 }
 if (pipe(cgi_input) < 0) {
  cannot_execute(client);
  return;
 }

 if ( (pid = fork()) < 0 ) {
  cannot_execute(client);
  return;
 }
 if (pid == 0)  /* child: CGI script */
 {
  char meth_env[255];
  char query_env[255];
  char length_env[255];

  dup2(cgi_output[1], 1);
  dup2(cgi_input[0], 0);
  close(cgi_output[0]);
  close(cgi_input[1]);
  sprintf(meth_env, "REQUEST_METHOD=%s", method);
  putenv(meth_env);
  if (strcasecmp(method, "GET") == 0) {
   sprintf(query_env, "QUERY_STRING=%s", query_string);
   putenv(query_env);
  }
  else {   /* POST */
   sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
   putenv(length_env);
  }
  execl(path, path, NULL);
  exit(0);
 } else {    /* parent */
  close(cgi_output[1]);
  close(cgi_input[0]);
  if (strcasecmp(method, "POST") == 0)
   for (i = 0; i < content_length; i++) {
    recv_wrap(client, &c, 1, 0);
    write(cgi_input[1], &c, 1);
   }
  while (read(cgi_output[0], &c, 1) > 0)
   send(client, &c, 1, 0);

  close(cgi_output[0]);
  close(cgi_input[1]);
  waitpid(pid, &status, 0);
 }
#endif
}

/**********************************************************************/
/* Get a line from a socket, whether the line ends in a newline,
 * carriage return, or a CRLF combination.  Terminates the string read
 * with a null character.  If no newline indicator is found before the
 * end of the buffer, the string is terminated with a null.  If any of
 * the above three line terminators is read, the last character of the
 * string will be a linefeed and the string will be terminated with a
 * null character.
 * Parameters: the socket descriptor
 *             the buffer to save the data in
 *             the size of the buffer
 * Returns: the number of bytes stored (excluding null) */
/**********************************************************************/
int get_line(int sock, char *buf, int size)
{
	int i = 0;
	char c = '\0';
	int n;

	while ((i < size - 1) && (c != '\n'))
	{
	n = recv_wrap(sock, &c, 1, 0);
	/* DEBUG printf("%02X\n", c); */
	if (n > 0)
	{
	if (c == '\r')
	{
	n = recv_wrap(sock, &c, 1, MSG_PEEK);
	/* DEBUG printf("%02X\n", c); */
	if ((n > 0) && (c == '\n'))
	 recv_wrap(sock, &c, 1, 0);
	else
	 c = '\n';
	}
	buf[i] = c;
	i++;
	}
	else
	c = '\n';
	}
	buf[i] = '\0';

	return(i);
}

/**********************************************************************/
/* Return the informational HTTP headers about a file. */
/* Parameters: the socket to print the headers on
 *             the name of the file */
/**********************************************************************/
void headers(int client, const char *filename)
{
#if 1
	char buf[1024];
	char contentType[128];
	char suffix[16];
	char *pos = NULL;
	int i;
	struct stat st;
	(void)filename;  /* could use filename to determine file type */


	/* �����ļ����� */
	pos = (char*)strrchr(filename, '.');
	if(pos != NULL)
	{
		pos++;
		for(i = 0; mime_tab[i].ext_len != 0; i++)
		{
			if(strcmp(pos, mime_tab[i].ext) == 0)
			{
				strcpy(contentType, mime_tab[i].value);
				break;
			}
		}
		if(mime_tab[i].ext_len == 0)
		{
			strcpy(contentType, "application/octet-stream");
		}
	}
	else
	{
		strcpy(contentType, "application/octet-stream");
	}
	/* ��ȡ�ļ���Ϣ */
	stat(filename, &st);
	
	LOGD("response[");
	strcpy(buf, "HTTP/1.1 200 OK\r\n");
	LOGD("%s", buf);
	send_wrap(client, buf, strlen(buf), 0);
	strcpy(buf, SERVER_STRING);
	LOGD("%s", buf); 
	send_wrap(client, buf, strlen(buf), 0);
	//sprintf(buf, "Content-Type: text/html\r\n");
	sprintf(buf, "Content-Type: %s\r\n", contentType);
	LOGD("%s", buf);
	send_wrap(client, buf, strlen(buf), 0);
	sprintf(buf, "Content-Length: %lld\r\n", st.st_size);
	LOGD("%s", buf);
	send_wrap(client, buf, strlen(buf), 0);
	sprintf(buf, "Connection: Keep-alive\r\n");
	LOGD("%s", buf);
	send_wrap(client, buf, strlen(buf), 0);
	strcpy(buf, "\r\n");
	LOGD("%s", buf);
	send_wrap(client, buf, strlen(buf), 0);
	LOGD("]");
	
#else
 char buf[1024] = "";
 int i = 0; 
 struct stat st;
 char *ext = strrchr(filename, '.')+sizeof('.');
 (void)filename;  /* could use filename to determine file type */
 stat(filename, &st);
 for (i = 0; mime_tab[i].ext != NULL; i++) 
 {
   if (strcmp(mime_tab[i].ext, ext) == 0) 
   {
   		snprintf(buf, 1024, "HTTP/1.1 206 Partial Content\r\nContent-Type: %s\r\nContent-Length: %lld\r\nContent-Ranges: bytes 0-%lld/%lld\r\nConnection: Keep-alive\r\n\r\n",
     		mime_tab[0].value, st.st_size, st.st_size, st.st_size);
   		LOGD("response[%s], mime_tab[%d].ext[%s]", buf, i, mime_tab[i].ext);
   		break;
   }
 }
 if (mime_tab[i].ext_len == NULL) 
 {
   snprintf(buf, 1024, "HTTP/1.0 200 OK\r\n"SERVER_STRING"Content-Type: text/html\r\n\r\n");
   LOGD("response[%s]", buf);
 }
 send(client, buf, strlen(buf), 0);
#endif
}

/**********************************************************************/
/* Give a client a 404 not found status message. */
/**********************************************************************/
void not_found(int client)
{
 char buf[1024];

 sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
 send_wrap(client, buf, strlen(buf), 0);
 sprintf(buf, SERVER_STRING);
 send_wrap(client, buf, strlen(buf), 0);
 sprintf(buf, "Content-Type: text/html\r\n");
 send_wrap(client, buf, strlen(buf), 0);
 sprintf(buf, "\r\n");
 send_wrap(client, buf, strlen(buf), 0);
 sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
 send_wrap(client, buf, strlen(buf), 0);
 sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
 send_wrap(client, buf, strlen(buf), 0);
 sprintf(buf, "your request because the resource specified\r\n");
 send_wrap(client, buf, strlen(buf), 0);
 sprintf(buf, "is unavailable or nonexistent.\r\n");
 send_wrap(client, buf, strlen(buf), 0);
 sprintf(buf, "</BODY></HTML>\r\n");
 send_wrap(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Send a regular file to the client.  Use headers, and report
 * errors to client if they occur.
 * Parameters: a pointer to a file structure produced from the socket
 *              file descriptor
 *             the name of the file to serve */
/**********************************************************************/
void serve_file(int client, const char *filename)
{
 	FILE *resource = NULL;
 	int numchars = 1;
 	char buf[1024];
	long long startPos = 0;		/*Range������ļ���ʼλ��*/
	long long endPos = 0;		/*Range������ļ�����λ��*/
	int hasRange = 0;
	struct stat st;
	

	/* �����ļ���ʼ/����λ�� */
	numchars = get_line(client, buf, sizeof(buf));		/* ��ȡ��һ������ */
	while ((numchars > 0) && strcmp("\n", buf))		
	{
		LOGD("%s\n", buf);
		if (strncasecmp(buf, "Range:", 6) == 0)			
		{
			LOGD("%s\n", buf);
			hasRange = 1;
			char *strRange;
			strRange = strchr(&buf[7], '=');	/* =1234-2345 */
			strRange++;							/* 1234 */
			char *strNum;
			strNum = strchr(strRange, '-');		/* -2345 */
			*strNum = '\0'; 
			strNum++;							/* 2345 */

			if(*strNum == '\r' || *strNum == '\n')/*����=1234-�����*/
			{
				LOGD("deal with =1234-");
				stat(filename, &st);
				startPos = atoll(strRange);
				endPos = (long long)st.st_size - 1; /*st_size�����д��ļ���������*/
			}
			else
			{
				startPos = atoll(strRange);
				endPos = atoll(strNum);
			}
			LOGD("the startPos = %lld\n", startPos);
			LOGD("the endPos = %lld\n", endPos);
		}
		numchars = get_line(client, buf, sizeof(buf));
	}


	resource = fopen(filename, "r");
	if (resource == NULL)
	{
		LOGD("resource is NULL\n");
		not_found(client);
	}
	else
	{
		if(hasRange == 1)
		{
			LOGD("doing serve_partial_file\n");
			/* ��ȡ�ļ���Ϣ */
			stat(filename, &st);
			
			/*�ж������Range�Ƿ�Ϸ�*/
			if(startPos < 0 || endPos >= st.st_size)	/* 0-1233/1234���ļ���0��ʼ�� */
			{
				LOGD("requested_range_not_satisfiable\n");
				requested_range_not_satisfiable(client);
			}
			else
			{
				LOGD("serve_partial_file\n");
				serve_partial_file(client, filename, resource, startPos, endPos, st.st_size);
			}
		}
		else
		{
			LOGD("serve_whole_file\n");
			serve_whole_file(client, filename, resource);
		}
	}

#if 0
 buf[0] = 'A'; buf[1] = '\0';
 while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
  numchars = get_line(client, buf, sizeof(buf));

 resource = fopen(filename, "rb");
 if (resource == NULL)
  not_found(client);
 else
 {
  headers(client, filename);
  cat(client, resource);
 }
#endif

 fclose(resource);
}


void requested_range_not_satisfiable(int sockfd)
{
	char buf[1024];

	sprintf(buf, "HTTP/1.1 416 Requested Range Not Satisfiable\r\n");
	LOGD("%s", buf);
	send_wrap(sockfd, buf, strlen(buf), 0);
	
	sprintf(buf, SERVER_STRING);
	LOGD("%s", buf);
	send_wrap(sockfd, buf, strlen(buf), 0);
	
	sprintf(buf, "Content-Type: text/html\r\n");
	LOGD("%s", buf);
	send_wrap(sockfd, buf, strlen(buf), 0);
	
	sprintf(buf, "\r\n");
	LOGD("%s", buf);
	send_wrap(sockfd, buf, strlen(buf), 0);
	
	sprintf(buf, "<HTML><TITLE>Requested Range Not Satisfiable</TITLE>\r\n");
	LOGD("%s", buf);
	send_wrap(sockfd, buf, strlen(buf), 0);
	
	sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
	LOGD("%s", buf);
	send_wrap(sockfd, buf, strlen(buf), 0);
	
	sprintf(buf, "your request because the range requested\r\n");
	LOGD("%s", buf);
	send_wrap(sockfd, buf, strlen(buf), 0);
	
	sprintf(buf, "is not satisfiable.\r\n");
	LOGD("%s", buf);
	send_wrap(sockfd, buf, strlen(buf), 0);
	
	sprintf(buf, "</BODY></HTML>\r\n");
	LOGD("%s", buf);
	send_wrap(sockfd, buf, strlen(buf), 0);
}

/* ��������ļ� */
void serve_whole_file(int sockfd, const char *filename, FILE *resource)
{
	headers(sockfd, filename);
	cat(sockfd, resource);
}

/* ���ز����ļ� */
void serve_partial_file(int sockfd, const char *filename, FILE *resource, long long startPos, long long endPos, long long fileSize)
{
	partial_headers(sockfd, filename, startPos, endPos);
	partial_cat(sockfd, resource, startPos, endPos, fileSize);
}

/* ���ز����ļ�ʱ����Ӧͷ�� */
void partial_headers(int sockfd, const char *filename, long long startPos, long long endPos)
{
	char buf[1024];
	char contentType[128];
	char suffix[16];
	char *pos = NULL;
	int i;
	//long long resstartPos = startPos;
	//long long resendPos = endPo
	struct stat st;
 	(void)filename;  /* could use filename to determine file type */


	/* �����ļ����� */
	pos = (char*)strrchr(filename, '.');
	if(pos != NULL)
	{
		pos++;
		for(i = 0; mime_tab[i].ext_len != 0; i++)
		{
			if(strcmp(pos, mime_tab[i].ext) == 0)
			{
				strcpy(contentType, mime_tab[i].value);
				break;
			}
		}
		if(mime_tab[i].ext_len == 0)
		{
			strcpy(contentType, "application/octet-stream");
		}
	}
	else
	{
		strcpy(contentType, "application/octet-stream");
	}

	/* ��ȡ�ļ���Ϣ */
	stat(filename, &st);

	strcpy(buf, "HTTP/1.1 206 Partial Content\r\n");
	LOGD("%s", buf);
	send_wrap(sockfd, buf, strlen(buf), 0);
	
	strcpy(buf, SERVER_STRING);
	LOGD("%s", buf);
	send_wrap(sockfd, buf, strlen(buf), 0);
	
	sprintf(buf, "Content-Type: %s\r\n", contentType);
	LOGD("%s", buf);
	send_wrap(sockfd, buf, strlen(buf), 0);
	
	sprintf(buf, "Content-Length: %lld\r\n", endPos-startPos+1);//�˴���Ӧ��Ϊ�ܴ�Сst.st_size����Ӧ��Ϊʵ�ʷ��صĳ���st.st_size);
	LOGD("%s", buf);
	printf("the st.st_size is %lld\n", st.st_size);
	send_wrap(sockfd, buf, strlen(buf), 0);

	sprintf(buf, "Content-Range: bytes %lld-%lld/%lld\r\n", startPos, endPos, st.st_size);
	LOGD("%s", buf);
	send_wrap(sockfd, buf, strlen(buf), 0);
	
	sprintf(buf, "Connection: Keep-Alive\r\n");
	LOGD("%s", buf);
	send_wrap(sockfd, buf, strlen(buf), 0);
	
	strcpy(buf, "\r\n");
	send_wrap(sockfd, buf, strlen(buf), 0);
}

/* ���ز�����Ӧʱ������� */
void partial_cat(int sockfd, FILE *resource, long long startPos, long long endPos, long long fileSize)
{
	char buf[4096];
	int i = 0;
	int readlen;
	int sendret;
	long long sendlength = endPos - startPos + 1; /* ������ݳ��� */

	/*�ļ�����2G��ʱ����м���������*/
	if(startPos <= 2147483647)
	{	/*˳��λ*/
		if(fseek(resource, startPos, SEEK_SET) != 0)
		{
			LOGD("[partial_cat] fseek error!\n");
		}
	}
	else
	{
		/*����λ*/
		long long rStartPos = (fileSize - 1) - startPos;
		if(fseek(resource, rStartPos, SEEK_END) != 0)
		{
			LOGD("[partial_cat] reverse fseek error!\n");
		}
		
	}


	while((readlen = fread(buf, sizeof(char), 4096, resource)))
	{
		//printf("i = %d, readlen = %d\n", ++i, readlen);
		if(sendlength < readlen)
		{
		//LOGD("i = %d, sendlength = %d\n", ++i, sendlength);
			
			sendret = send_wrap(sockfd, buf, sendlength, 0);
			if(sendret < 0)
			{
				LOGD("[partial_cat] send error! [%s]\n", strerror(errno));
				return;
			}
			sendlength -= sendlength;
		}
		else
		{
		//LOGD("i = %d, readlen = %d\n", ++i, readlen);

			sendret = send_wrap(sockfd, buf, readlen, 0);
			if(sendret < 0)
			{
				LOGD("[partial_cat] send error! [%s]\n", strerror(errno));
				return;
			}
			++i;
			
			//if(i%500 == 0)
			//{
//				struct timeval t_timeval;
//				t_timeval.tv_sec = DOWNLOAD_SPEED_SEC;
//				t_timeval.tv_usec = DOWNLOAD_SPEED_MICRO_SEC;
				usleep(50000);
				//select(0, NULL, NULL, NULL, &t_timeval);
			//}
			
			sendlength -= readlen;
		}
		if(sendlength == 0)
		{
			/* ��������� */
			//LOGD("sendlength == 0\n");
			break;
		}
	}
	/* ����������ݻ��ߴ��� */
	if(ferror(resource))
	{
		LOGD("[partial_cat] end before eof\n");
		return;
	}

	LOGD("send partial file finish");
	
}


/**********************************************************************/
/* This function starts the process of listening for web connections
 * on a specified port.  If the port is 0, then dynamically allocate a
 * port and modify the original port variable to reflect the actual
 * port.
 * Parameters: pointer to variable containing the port to connect on
 * Returns: the socket */
/**********************************************************************/
int startup(u_short *port)
{
	int httpd = 0;
	struct sockaddr_in name;

	httpd = socket(PF_INET, SOCK_STREAM, 0);
	if (httpd == -1)
		error_die("socket");
	memset(&name, 0, sizeof(name));
	name.sin_family = AF_INET;
	name.sin_port = htons(*port);
	name.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(httpd, (struct sockaddr *)&name, sizeof(name)) < 0)
		error_die("bind");
	if (*port == 0)  /* if dynamically allocating a port */
	{
		int namelen = sizeof(name);
		if (getsockname(httpd, (struct sockaddr *)&name, (socklen_t*)&namelen) == -1)
			error_die("getsockname");
		*port = ntohs(name.sin_port);
	}
	if (listen(httpd, 10) < 0)
		error_die("listen");
	return(httpd);
}

/**********************************************************************/
/* Inform the client that the requested web method has not been
 * implemented.
 * Parameter: the client socket */
/**********************************************************************/
void unimplemented(int client)
{
	char buf[1024];

	sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
	send_wrap(client, buf, strlen(buf), 0);
	sprintf(buf, SERVER_STRING);
	send_wrap(client, buf, strlen(buf), 0);
	sprintf(buf, "Content-Type: text/html\r\n");
	send_wrap(client, buf, strlen(buf), 0);
	sprintf(buf, "\r\n");
	send_wrap(client, buf, strlen(buf), 0);
	sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
	send_wrap(client, buf, strlen(buf), 0);
	sprintf(buf, "</TITLE></HEAD>\r\n");
	send_wrap(client, buf, strlen(buf), 0);
	sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
	send_wrap(client, buf, strlen(buf), 0);
	sprintf(buf, "</BODY></HTML>\r\n");
	send_wrap(client, buf, strlen(buf), 0);
}

/**********************************************************************/
void* acceptThread(void *argv)
{
	int server_sock = -1;

#ifdef SDV
	u_short port = 80;
#else
	u_short port = 8082;
#endif
	int client_sock = -1;
	struct sockaddr_in client_name;
	int client_name_len = sizeof(client_name);
	pthread_t newthread;

	server_sock = startup(&port);
	printf("httpd running on port %d\n", port);
	startFlag = 1;
	while(startFlag == 1)
	{
		fd_set rfd;
		FD_ZERO(&rfd);
		FD_SET(server_sock, &rfd);
		struct timeval to = {1, 0};
		int selectResult = select(server_sock+1, &rfd, NULL, NULL, &to);
		if (selectResult > 0) 
		{
			client_sock = accept(server_sock, (struct sockaddr *)&client_name, (socklen_t*)&client_name_len);
			if (client_sock == -1)
			{
				perror("accept");
				continue;
			}
			LOGD("accept success, client_sock:%d", client_sock);
			/* accept_request(client_sock); */
			int *client_sock_ptr = new int;
			*client_sock_ptr = client_sock;
//			if (pthread_create(&newthread, NULL, accept_request, (void*)client_sock_ptr) != 0)
//			{
//				perror("pthread_create");
//				continue;
//			}
//			pthread_detach(newthread);
			accept_request(client_sock_ptr);
		}
		else if(selectResult < 0)
		{
			break;
		}
	}
	startFlag = 0;
	close(server_sock);

	return(0);
}

int startHttpd(httpdCallback *callback)
{
	/*����SIGPIPE�ź�*/
	signal(SIGPIPE, SIG_IGN);
	printf("block SIGPIPE\n");
	if (startFlag == 1) {
		return -1;
	}

	if(callback)
	{
		gHttpdCallback.httpdDataCallback = callback->httpdDataCallback;
		gHttpdCallback.httpdStatCallback = callback->httpdStatCallback;
		gHttpdCallback.handler = callback->handler;
	}
	if(pthread_create(&acceptThreadId, NULL, acceptThread, NULL) != 0) {
		perror("pthread_create");
	}
	return 0;
}

int stopHttpd()
{
	startFlag = 0;
	pthread_join(acceptThreadId, NULL);
	gHttpdCallback.httpdDataCallback = NULL;
	gHttpdCallback.httpdStatCallback = NULL;
	gHttpdCallback.handler = NULL;
	return 0;
}

