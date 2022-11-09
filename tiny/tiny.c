/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize, char *method);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);


// main 함수의 argv[1]이 포트번호로 사용될 것이기 때문에 실행할때 ./tiny 포트번호 와 같이 실행해줘야 한다.
int main(int argc, char **argv)
{
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  printf("Tiny is on! ");
  /* Check command line args */
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  /* 입력한 포트번호로 소켓을 생성하고 bind, listen 하여 리스닝 소켓 생성 */
  listenfd = Open_listenfd(argv[1]);
  
  while (1)
  {
    clientlen = sizeof(clientaddr);

    /* accept로 클라이언트에서 들어오는 connect 요청을 기다리고 연결. 연결에 성공하면 clientaddr로 클라이언트 정보가 채워진다. */
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); //
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    
    /* 연결이 establised 되었으므로 transaction 시작*/
    doit(connfd);
    /* 트랜잭션이 끝나면 연결 종료 -> 다시 반복문으로 돌아가서 accept 로 새 연결 기다릴 것 */
    Close(connfd);
  }
}

/* a HTTP request를 다루는 함수 doit */
void doit(int fd){
  int is_static ;
  struct stat sbuf ;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  /* 1. request line을 읽고 parse */
  Rio_readinitb(&rio, fd); // 파일 식별자 fd와 buffer인 rio를 연결 (fd로 들어오는 정보는 rio 에 저장될 것)
  Rio_readlineb(&rio, buf, MAXLINE); // rio의 request line을 읽어서 buf로 복사(최대 MAXLINE-1 개의 바이트를 읽음)
  printf("Request headers : \n");
  printf("%s", buf); // buffer (들어온 파일 내용이 저장) 를 한 글자씩 출력
    /* 아래 와 같이 request line이 출력됨
        Request headers :
        GET / HTTP/1.1
    */
  sscanf(buf, "%s %s %s", method, uri, version) ;// sscanf 함수의 매개변수는 문자열 버퍼 - 버퍼의 값을 method, uri, version 에 parsing 하여 입력
   /* 2. TINY는 GET 요청만 받을 수 있으므로 메서드가 GET이 아니면
    *   - error를 보내고 main routine (main 함수) 으로 보냄
    *   - 연결을 종료하고 다음 연결 요청을 기다림
    * strcasecmp는 대소문자 비교 없이 두 개의 스트링을 비교. 같을 때는 0을 반환하므로 같지 않다면 if문 조건에서 참이 될 것  */
  if (strcasecmp(method, "GET")) {
    if (strcasecmp(method, "HEAD")){
      clienterror(fd, method, "501", "Not implemented", "Tiny implement only GET method");
      return ;
      }
    else {
    }
  }
  /* 3. 메서드가 GET이라면 read_requesthdrs 함수를 사용하여 request header를 읽는다 */
  read_requesthdrs(&rio);

  /* 4. URI를 filename과 CGI argument string 으로 parse 하고
   * 요청이 static인지 dynamic 인지를 구분하는 flag 생성
   * parse_uri 함수는 인자로 받은 uri를 parse해서 filename과 cgiargs에 각각 값을 입력해주고
   * static이면 1을, dynamic이면 0을 반환한다. */
  is_static = parse_uri(uri, filename, cgiargs);
  printf("parse_uri 실행?! ");
 /* 5. filename을 가진 파일이 디스크에 존재하지 않으면 에러 메세지 전송 후 return
  * stat은 문자열 파일명, stat 구조체 포인터를 인수로 받아 파일 정보를 읽어오는 함수
  * stat 함수는 파일을 찾으면 0을 반환하며 두번째 인자인 stat 구조체인 sbuf에 파일 정보가 채워진다.
  * 실패 혹은 에러시 -1을 리턴하고 에러시에 errno 변수에 에러 상태가 set된다. */
  if (stat(filename, &sbuf) < 0 ) {
    clienterror(fd, filename, "404", "File Not found", "Tiny couldn't find this file.");
    return ;
  }

 /* 7. 요청이 static content 이면  */
 /*    파일이 일반 파일인지 & 파일에 대해 읽기 권한이 있는지 확인 후 (if so) 클라이언트로 static content를 serve */
if (is_static) {
  if(!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) { // ISREG는 is regular -> 레귤러 파일이 '아니거나', IRUSR은 USR가 R 권한이 있는지 -> 읽기 권한이 '없으면' => 에러
    clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read this file.");
    return ;
  }
  serve_static(fd, filename, sbuf.st_size, method); // filename과 argument가 parsing

}
 /* 8. 요청이 dynamic content 이면  */
 /*    파일이 실행가능한지(executable) 확인 후 (if so) dynamic content를 serve */
else {
  if(!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) { // ISREG는 is regular -> 레귤러 파일이 '아니거나', IXUSR은 USR가 X(실행) 권한이 있는지 -> 읽기 권한이 '없으면' => 에러
    clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program.");
    return ;
  }
  serve_dynamic(fd, filename, cgiargs);
}
};

/* 에러가 발생했을 때 HTTP response line에 상태코드와 상태 메세지를 포함하고,
 * response body 내에 에러 내용을 설명하는 html 파일을 포함하여 브라우저에 보내주는 함수
 * HTML response의 headers는 항상 content type과 content length를 포함해야 하므로 HTML content를 single string으로 만들어 길이를 계산하기 쉽게 한다.
 */
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg){
  char buf[MAXLINE], body[MAXBUF];

  /* HTTP response body 를 생성
   * sprintf() 함수는 배열 버퍼에 일련의 문자와 값의 형식을 지정하고 저장 */
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);
  
  /* HTTP response */
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf)); // buf에서 fd로 데이터 복사
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf)); // buf에서 fd로 데이터 복사
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf)); // buf에서 fd로 데이터 복사
  Rio_writen(fd, body, strlen(body)); // body에서 fd로 데이터 복사
};

/* request header 정보를 읽는 함수 */
void read_requesthdrs(rio_t *rp){
  char buf[MAXLINE];
  Rio_readlineb(rp, buf, MAXLINE); // rp의 내용을 읽고 buf로 복사
  while(strcmp(buf, "\r\n")) { // buf에 빈문자열(\r\n 발생)이 나올 때까지 계속 읽어들인다
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s\n", buf);
  }
  return ;
};

/* URI를 parse 하여 static인지, dynamic 인지 구분. uri를 받은 후 filename과 cgiargs 에 parsed 값을 저장해 준다 */
int parse_uri(char *uri, char *filename, char *cgiargs){
  char *ptr, *pathstart, tmpfilename[MAXLINE];
  int tmp;

  if (!strstr(uri, "cgi-bin")) { // strstr: 서브스트링을 찾는 함수. uri에 cgi-bin라는 스트링이 없다면 (= static content)
    strcpy(cgiargs, ""); // cgi argument 클리어
    if (strncmp(uri, "http://", 7) == 0)
    {
        strcpy(tmpfilename, uri+7);
    }
    else
    {
        strcat(tmpfilename, uri);
    }
  
    if (uri[strlen(uri)-1] == '/')
        strcat(tmpfilename, "home.html"); // filename(경로)에 home.html 붙임
    
    strcpy(filename, "."); // filename에 . 복사
    pathstart= index(tmpfilename, '/');
    if(pathstart) {
      strcat(filename, pathstart);
    }
    printf("%s",filename);
    return 1;
  }
  /* 요청이 dynamic content 라면 */ 
  else {
    ptr = index(uri, '?'); // URI에서 ?가 시작하는 부분을 ptr로 저장
    if(ptr) {  // query string이 존재한다면
      strcpy(cgiargs, ptr+1);  //여기서 인수로 들어간 ptr+1은 메모리 주소. ptr+1부터 (? 이후부터) query string을 cgiargs로 복사
      *ptr = '\0';   //'\0'는 NULL ;
    }
    else // URI에 query string이 없다면
      strcpy(cgiargs, ""); // argument는 클리어
    strcpy(filename, "."); // filename에 . 복사
    strcat(filename, uri); // . 뒤에 uri 이어 붙이기
    return 0 ;
  }
};

/* serve_static은 요청 받은 로컬 파일 내용을 body에 포함해서 HTTP response를 보내는 함수 */
void serve_static(int fd, char *filename, int filesize, char *method){
  int srcfd ;
  char  head;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

 /* TINY는 HTML files, unformatted text files, and images encoded in GIF, PNG, and JPEG formats 의 5가지 static content를 serve
 * get_filetype 함수는 filename의 suffix를 조사하여 파일의 타입을 조사 하고, response line 과 response headers를 client로 보낸다 */
  get_filetype(filename, filetype);
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  Rio_writen(fd, buf, strlen(buf)); // buf를 fd로 복사 (fd는 서버의 connfd) 즉, conn소켓에서 클라이언트 소켓으로 전송
  printf("Response headers: \n");
  printf("%s", buf);

  if (head = strcasecmp(method, "HEAD") == 0 ){
    return;
  }

 /*  이후 요청 받은 파일의 내용을 copy해서 연결식별자 fd 로 보내는 방식으로 response body를 보낸다. */
 /* mmap 함수는 요청 받은 파일과 가상 메모리 영역을 매핑한다. 메모리에 매핑된 식별자는 더이상 필요하지 않으므로 파일을 닫는다 (메모리 누수 방지) */
 /* rio_writen 함수를 사용해서 파일이 매핑된 가상메모리 주소부터 파일 사이즈 만큼 복사해서 클라이언트로 보낸다*/
 /* 매핑된 메모리 영역을 해제한다 (메모리 누수 방지)*/
  srcfd = Open(filename, O_RDONLY, 0); // 읽기 전용으로 해당 file을 열고 파일 식별자를 저장
  printf("%s", filename);

  // srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
  srcp = malloc(filesize);
  rio_readn(srcfd, srcp, filesize);
  Close(srcfd); // 파일 닫기
  Rio_writen(fd, srcp, filesize); // scrp 부터 파일사이즈 만큼 읽어서 fd로 보낸다. 즉, conn소켓에서 클라이언트 소켓으로 전송
  // Munmap(srcp, filesize); // 메모리 매핑 해제
  free(srcp); // 메모리 매핑 해제
};

/* file name의 확장자가 있는지 보고, filetype에 파일 타입 값 저장*/
void get_filetype(char *filename, char *filetype){
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html"); // 파일 타입으로 MIME type 저장
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif"); // 파일 타입으로 MIME type 저장
  else if (strstr(filename, ".png"))
    strcpy(filetype, "image/png"); // 파일 타입으로 MIME type 저장
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg"); // 파일 타입으로 MIME type 저장
  else if (strstr(filename, ".mp4"))
    strcpy(filetype, "video/mp4"); // 파일 타입으로 MIME type 저장
  else
    strcpy(filetype, "text/plain"); // 파일 타입으로 MIME type 저장
};

void serve_dynamic(int fd, char *filename, char *cgiargs){
  char buf[MAXLINE], *emptylist[]= {NULL};
  
  /* 우선 클라이언트로 Server header에 성공 정보를 담아서 응답 (HTTP response) 이후 작업은 CGI 프로그램이 실행 */
  sprintf(buf, "HTTP/1.0 200 OK\r\n"); //buf에 쓰고
  Rio_writen(fd, buf, strlen(buf)); //buf 내용을 클라이언트에 보낸다
  sprintf(buf, "Server: Tiny Web Server\r\n"); //buf에 쓰고
  Rio_writen(fd, buf, strlen(buf)); //buf 내용을 클라이언트에 보낸다

  /* dynamic content를 serve 하기 위해서 child process를 fork 한 후 CGI 를 실행한다 */
  if (Fork() ==0){  // 새 child process를 fork
    // 아래는 child 에서 작업
    setenv("QUERY_STRING", cgiargs, 1);  // cgi 프로그램의 환경변수 QUERY_STRING를 URI에서 parse해온 cgiars로 초기화 (실제 서버는 이 이외의 다른 환경변수들도 설정한다)
    Dup2(fd, STDOUT_FILENO); // child의 standard output을 fd로 redirect 시킨다 (이에 따라 실행되는 output이 fd로 바로 전달된다)
    Execve(filename, emptylist, environ); // CGI 프로그램을 로드하고 실행 (child 에서 실행되기 때문에 evecve 실행 전에 존재했던 환경 변수들과 열린 파일에 접근이 가능하다)
  }
};

