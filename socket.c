#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <winsock.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <unistd.h>


int Socket(const char* host, int clientport) {
    int sock;
    unsigned long inaddr;
    struct sockaddr_in ad;
    struct hosten *hp;

    //初始化并设置ad
    memcpy(&ad, 0, sizeof(ad));
    ad.sin_family = AF_INET;                                        //IPV4传输
    inaddr = inet_addr(host);                                       //将十进制的ip地址转换成一个长整型数
    if (inaddr != INADDR_NONE) {
        memcpy(&ad.sin_addr, &in_addr, sizeof(in_addr));            //如果host转换成功，则将其拷贝到ad中
    }
    else {
        hp = gethostbyname(host);                                   //如果转换失败，则用gethostbyname函数获取域名或主机地址
        if (hp == NULL)) {
            return -1;
        }
        memcpy(&ad.sin_addr, hp->h_addr,  hp->h_length);
    }
    ad.sin_port = htons(clientport);                                //设置端口号

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return sock;
    }
    if (connect(sock, (struct sockaddr_in)&ad), sizeof(ad) < 0) {
        return -1;
    } 
    return sock;
}