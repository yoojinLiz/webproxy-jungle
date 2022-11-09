#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

void doit(int fd);
void handle_rqdrs(rio_t *rp, int clientfd);
int parse_uri(char *uri, char *filename, char *serverport);
int connect_server(rio_t *hdr, char *port);
void forward(rio_t *rp, int clientfd, int fd);
void get_filetype(char *filename, char *filetype);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

int main(int argc, char **argv)
{
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    printf("%s", user_agent_hdr);

    /* Check command line args */
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    listenfd = Open_listenfd(argv[1]);

    while (1)
    {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); // 연결에 성공하면 clientaddr로 클라이언트 정보가 채워진다. */
        Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port); //연결이 establised 되었음
        doit(connfd);
        Close(connfd);
    }
    return 0;
}

void doit(int fd) // http 요청을 받아 처리한다
{
    int clientfd;
    char *host, *port;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE], filename[MAXLINE], serverport[6];
    rio_t rio, hdr; // request header 담을 버퍼

    /* request line을 읽고 parse */
    Rio_readinitb(&rio, fd);           // 파일 식별자 fd와 buffer인 rio를 연결 (fd로 들어오는 정보는 rio 에 저장될 것)
    Rio_readlineb(&rio, buf, MAXLINE); // rio의 request line을 읽어서 buf로 복사(최대 MAXLINE-1 개의 바이트를 읽음)
    printf("Request line : \n");
    printf("%s", buf);                             // buffer (들어온 파일 내용이 저장) 를 한 글자씩 출력  -> GET / HTTP/1.1
    sscanf(buf, "%s %s %s", method, uri, version); // sscanf 함수의 매개변수는 문자열 버퍼 - 버퍼의 값을 method, uri, version 에 parsing 하여 입력
    /* 정상적인 요청인지 확인 - TINY는 GET, HEAD 만 받을 수 있음  */
    if (strcasecmp(method, "GET"))
    {
        if (strcasecmp(method, "HEAD"))
        {
            clienterror(fd, method, "501", "Not implemented", "Tiny implement only GET method");
            return;
        }
    }
    /* URI에서 / 로 끝나면 home.html 붙도록 처리  */
    parse_uri(uri, filename, serverport);

    /* 서버와 연결 확립 후, 헤더를 request line 및 request header 전송 */
    clientfd = connect_server(&hdr, serverport); // 이 hdr 버퍼와 client는 연결된 상태

    sprintf(buf, "%s %s %s", method, uri, "HTTP/1.0");
    Rio_writen(clientfd, buf, strlen(buf)); // buffer의 내용 GET / HTTP/1.0 이를 서버에 보낸다!

    handle_rqdrs(&rio, clientfd); // 헤더 읽고 서버로 요청 보내기
    forward(&rio, clientfd, fd); // 응답 받은 내용을 connfd인 fd로 전송
    Close(clientfd);
    exit(0);
};

/* 에러가 발생했을 때 HTTP response line에 상태코드와 상태 메세지를 포함하고,
 * response body 내에 에러 내용을 설명하는 html 파일을 포함하여 브라우저에 보내주는 함수
 * HTML response의 headers는 항상 content type과 content length를 포함해야 하므로 HTML content를 single string으로 만들어 길이를 계산하기 쉽게 한다.
 */
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
    char buf[MAXLINE], body[MAXBUF];

    /* HTTP response body 를 생성
     * sprintf() 함수는 배열 버퍼에 일련의 문자와 값의 형식을 지정하고 저장 */
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor="
                  "ffffff"
                  ">\r\n",
            body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

    /* HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf)); // buf에서 fd로 데이터 복사
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf)); // buf에서 fd로 데이터 복사
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));   // buf에서 fd로 데이터 복사
    Rio_writen(fd, body, strlen(body)); // body에서 fd로 데이터 복사
};

/* request header 정보를 읽는 함수 */
void read_and_write_requesthdrs(rio_t *rp)
{
    char buf[MAXLINE];
    Rio_readlineb(rp, buf, MAXLINE); // rp의 request header 내용을 읽고 buf로 복사
    while (strcmp(buf, "\r\n"))
    {                                    // buf에 빈문자열(\r\n 발생)이 나올 때까지
        Rio_readlineb(rp, buf, MAXLINE); // 계속 읽고 buf로 복사
        printf("\nI am reqest header %s \n", buf);
    }
    return;
};

int parse_uri(char *uri, char *filename, char *serverport)
{
    char *portstart, *portend, tmpprtname[MAXLINE];
    int i, tmp, portlen; 
    int size = sizeof(uri)/sizeof(uri[0]);

    if (strncmp(uri, "http://", 7) == 0)
    {
        strcpy(filename, uri+7); // filename에 http:// 뒤부터 복사
    }
    else
    {
        strcat(filename, uri); // 이미 http:// 가 있다면 뒤에 uri만 이어 붙이기
    }  
    // tmp = index(uri, "http://");
    // /* 만약 URI내에 http:// 라는 스트링이 있다면 없애자  */
    // if(tmp != NULL ) {
    //     strcpy(filename, uri+7); // filename에 http:// 뒤부터 복사
    // }
    // else
    //     strcat(filename, uri); // 이미 http:// 가 있다면 뒤에 uri만 이어 붙이기
    if (uri[strlen(uri) - 1] == '/')   // uri가 /로만 끝난다면
        strcat(filename, "home.html"); // filename(경로)에 home.html 붙임
    portstart= index(filename, ':'); 
    if(portstart) {
        portend= index(filename, '/');
        portlen = portend - portstart -1 ;
        for (i=0; i<portlen; i++)
        strncpy(serverport, portstart+1, portlen);
    }
    else 
        strcpy(serverport, "80"); // 포트번호가 없다면 80 포트로  
    printf("serverport is %s \n", serverport);
    return 0;
};

/* forward은 응답 받은 HTTP response를 클라이언트로 전달하는 함수 */
void forward(rio_t *rp, int clientfd, int fd)
{//이미 clientfd와 연결되어있는 rp에는 데이터가 들어와 있다. 이를 fd로 전달하면 됨 
    char buf[MAXLINE];
    Rio_readn(clientfd, buf, MAXLINE);
    Rio_writen(fd, buf, strlen(buf));
};

int connect_server(rio_t *hdr, char *port)
{
    int clientfd;
    char *host;
    printf("port number is %s \n", port);
    host = "15.164.173.214"; // 이렇게 서버 주소가 노출되면 안될 거 같긴한데(?)
    clientfd = Open_clientfd(host, &port);
    Rio_readinitb(hdr, clientfd);
    return clientfd;
};

void handle_rqdrs(rio_t *rp, int clientfd)
{
    char buf[MAXLINE];
    char hosthdr = 0;
    char userhdr = 0;
    Rio_readlineb(rp, buf, MAXLINE); // rp의 내용을 읽고 buf로 복사
    printf("%s", buf);

    while (strcmp(buf, "\r\n")) // buf에 빈문자열(\r\n 발생)이 나올 때까지 계속 읽어들인다
    {
        if (hosthdr = strstr(buf, "Host") != NULL)
        {
            hosthdr = 1;
            Rio_writen(clientfd, buf, strlen(buf)); // buf를 clientfd로 write (서버로 보낸다)
        }
        if (userhdr = (strstr(buf, "User-Agent")) != NULL)
        {
            Rio_writen(clientfd, buf, strlen(buf)); // buf를 clientfd로 write (서버로 보낸다)
        }
        Rio_readlineb(rp, buf, MAXLINE);
        printf("%s", buf); 
    }
    if (hosthdr == 0)
    {
        sprintf(buf, "%s", "Host: 15.164.173.214\r\n");
        Rio_writen(clientfd, buf, strlen(buf)); // buffer의 내용 GET / HTTP/1.0 이를 서버에 보낸다!
    }
    sprintf(buf, "%s", "Connection: close\r\n");
    Rio_writen(clientfd, buf, strlen(buf)); //
    sprintf(buf, "%s", "Proxy-Connection: close\r\n");
    Rio_writen(clientfd, buf, strlen(buf)); //
    sprintf(buf, "%s", "\r\n");
    Rio_writen(clientfd, buf, strlen(buf)); //
    return;
};
