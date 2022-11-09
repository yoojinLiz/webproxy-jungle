/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

/* dynamic content를 serving하는 CGI 프로그램 */
int main(void) {
  char *buf, *p; 
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1=0, n2=0;

  if ((buf = getenv("QUERY_STRING")) != NULL) { // 환경변수 QUERY_STRING 값을 buf로 복사해둔다 
    p = strchr(buf, '&'); // buf에서 '&'를 찾아서 그 위치를 리턴 
    *p = '\0'; // &를 NULL로 바꿔준다 (이 NULL값이 아래 strcpy를 할 때 값들을 나눠주는 구분자 역할을 할 것)

    strcpy(arg1, buf+2); //buf의 문자열 전체(== &가 있던 자리까지)를 arg1 에 복사 (NULL 포함)
    strcpy(arg2, p+3); // &뒤 자리부터 끝까지 arg2에 복사 (NULL 포함)
    n1= atoi(arg1); // atoi는 문자 스트링을 정수로 변환 (arg1의 문자열 값이 정수가 되는 것 )
    n2= atoi(arg2); // atoi는 문자 스트링을 정수로 변환 (arg2의 문자열 값이 정수가 되는 것 )
    // *p = '\0'; // &를 NULL로 바꿔준다 (이 NULL값이 아래 strcpy를 할 때 값들을 나눠주는 구분자 역할을 할 것)
    // strcpy(arg1, buf); //buf의 문자열 전체(== &가 있던 자리까지)를 arg1 에 복사 (NULL 포함)
    // strcpy(arg2, p+1); // &뒤 자리부터 끝까지 arg2에 복사 (NULL 포함)
    // n1= atoi(arg1); // atoi는 문자 스트링을 정수로 변환 (arg1의 문자열 값이 정수가 되는 것 )
    // n2= atoi(arg2); // atoi는 문자 스트링을 정수로 변환 (arg2의 문자열 값이 정수가 되는 것 )
  }

  /* Make the response body */
  sprintf(content, "QUERY_STING=%s", buf);
  sprintf(content, "Welcome to add.com: ");
  sprintf(content, "%sTHE Internet addition porter.\r\n<p>", content);
  sprintf(content, "%sThe answer is: %d + %d = %d\r\n<p>", content, n1, n2, n1+n2);
  sprintf(content, "%sThanks for visiting!\r\n", content);

  /* HTTP response 발생 */
  printf("Connection: close\r\n");
  printf("Content-length: %d\r\n", (int)strlen(content));
  printf("Content-type: text/html\r\n\r\n");
  printf("%s", content);
  fflush(stdout);

  exit(0);
}
/* $end adder */
