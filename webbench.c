#include <winsock.h>
#include "Socket.c"
#include <unistd.h>
#include <sys/param.h>
#include <rpc/types.h>
#include <getopt.h>
#include <strings.h>
#include <time.h>
#include <signal.h>



volatile int timerexpired = 0;                              //压测时间到达标志位
int speed = 0;                                              //存放连接成功次数
int failed = 0;                                             //存放连接失败次数
int bytes = 0;                                              //存放字节数

int http10 = 1;
#define METHOD_GET 0
#define METHOD_HEAD 1
#define METHOD_OPTION 2
#define METHOD_TRACE 3
#define PROGRAM_VERSION "1.5"
#define REQUEST_LENGTH 2048
int method = METHOD_GET;                                    //默认请求方式为get
int client = 1;                                             //默认客户端数量为1
int force = 0;                                              //是否等待服务器响应数据。 0：等待 1：不等待
int force_reload = 0;                                       //是否进行页面缓存。 0：缓存 1：不缓存
int proxyport = 80;                                         //默认连接端口80
int* proxyhost = NULL;                                      //代理服务器
int benchtime = 30;                                         //压测时间为30s

int mypipe[2];
char host[MAXHOSTNAMELEN];                                  //存放host
char request[REQUEST_LENGTH];                               //存放http请求数据

static const struct option long_options[] = 
{
    {"force", no_argument, &force, 1},
    {"reload", no_argument, &force_reload, 1},
    {"time", no_argument, NULL, "t"},
    {"help", no_argument, NULL, "?"},
    {"http09", no_argument, NULL, "9"},
    {"http10",no_argument,NULL,'1'},
    {"http11",no_argument,NULL,'2'},
    {"get",no_argument,&method,METHOD_GET},
    {"head",no_argument,&method,METHOD_HEAD},
    {"options",no_argument,&method,METHOD_OPTIONS},
    {"trace",no_argument,&method,METHOD_TRACE},
    {"version",no_argument,NULL,'V'},
    {"proxy",required_argument,NULL,'p'},
    {"clients",required_argument,NULL,'c'},
    {NULL,0,NULL,0}
};

static void build_request(const char* url);
static void bench(void);
static void benchcore(const char* host, const int port, const char* request);

static void alarm_handler(int signal) {                     //压测时间到
    timerexpired = 1;
}

static void usage(void) {                                   //若输入命令不对，则读取usage函数
    fprintf(stderr,
            "webbench [option]... URL\n"
            "  -f|--force               Don't wait for reply from server.\n"
            "  -r|--reload              Send reload request - Pragma: no-cache.\n"
            "  -t|--time <sec>          Run benchmark for <sec> seconds. Default 30.\n"
            "  -p|--proxy <server:port> Use proxy server for request.\n"
            "  -c|--clients <n>         Run <n> HTTP clients at once. Default one.\n"
            "  -9|--http09              Use HTTP/0.9 style requests.\n"
            "  -1|--http10              Use HTTP/1.0 protocol.\n"
            "  -2|--http11              Use HTTP/1.1 protocol.\n"
            "  --get                    Use GET request method.\n"
            "  --head                   Use HEAD request method.\n"
            "  --options                Use OPTIONS request method.\n"
            "  --trace                  Use TRACE request method.\n"
            "  -?|-h|--help             This information.\n"
            "  -V|--version             Display program version.\n"
    );
}

int main(int argc, char* argv[]) {
    int opt = 0;
    int option_index = 0;
    char* tmp = NULL;                                                       //存放代理服务器的端口号

    if (argc == 1) {
        usage();
        return 2;
    }

    while (opt = getopt_long(argc, argv, "912Vfrt:p:c:?h", long_options, &option_index) != EOF) {   //解析命令行参数
        switch(opt) {
            case 0 : break;
            case 'f' : force = 1;                break;                         //不等待服务器返回
            case 'r' : force_reload = 1;         break;                         //不进行页面缓存
            case '9' : http10 = 0;               break;
            case '1' : http10 = 1;               break;
            case '2' : http10 = 2;               break;
            case 'V' : printf(PROGRAM_VERSION"\n"); exit(0);
            case 't' : benchtime = atoi(optarg); break;
            case 'p' :                                                          //使用代理服务器时，设置代理服务器和端口号
                    tmp = strrchr(optarg, ':');
                    proxyhost = optarg;
                    if (tmp == NULL) {
                        break;
                    }
                    if (tmp == optarg) {
                        fprintf(stderr, "proxy error %s, no hostname\n", optarg);
                        return 2;
                    }
                    if (tmp == optarg + strlen(optarg) - 1) {
                        fprintf(stderr, "proxy error %s, no port number\n", optarg);
                        return 2;
                    }
                    *tmp = '\0';
                    proxyport = atoi(tmp + 1);
                    break;
            case ':' :
            case 'h' :
            case '?' : usage(); return 2;       break;
            case 'c' : client = atoi(optarg);   break;
        }
    }

    if (optind == argc) {                           //optind在解析完所有参数后应当指向url
        fprintf(stderr, "missing url!\n");
        usage();
        return 2;
    }
    if (client == 0) {
        client = 1;
    }
    if (benchtime == 0) {
        benchtime = 30;
    }

    fprintf(stderr,"Webbench - Simple Web Benchmark "PROGRAM_VERSION"\n"
            "Copyright (c) Radim Kolar 1997-2004, GPL Open Source Software.\n"
            );
 
    build_request(argv[optind]);                    //根据url建立http报文
    printf("Running info: ");
    if (client == 1) {
        printf("1 client");
    }
    else {
        printf("%d client", client);
    }
    printf(", Running %d seconds", benchtime);
    if (force) {
        printf(", early close socket");
    }
    if (force_reload) {
        printf(", no page-cache");
    }
    if (proxyhost != NULL) {
        printf(", proxyserver: %s. %d", proxyhost, proxyport);
    }
    printf("\n");
    return bench();
}

/**************************************************************/
/* 此函数主要把类似于http GET请求的信息全部存储到全局变量request
 * [REQUEST_SIZE]中，其中换行操作符使用“\r\n”,其中应用了大量的字
 * 符操作符函数。创建url请求连接，http头，创建好的请求放到request
 * 请求包格式分为使用代理和未使用代理两种。
 * 
 * 未使用代理：
 * 请求方式（get post head） /（表示请求路径） HTTP/1.0（协议版本）
 * User-Agent： ******* （用户身份） 
 * Host： （主机域名）                                         
 * （需要有空行）
 * 
 * 使用代理：
 * 请求方式（get post head） URL() HTTP协议版本
 * User-Agent: (用户身份)                                      
 * （需要有空行）                                              
 */
/**************************************************************/

void build_request(const char* url) {
    int i;
    char tmp[10];

    memset(host, 0, MAXHOSTNAMELEN);
    memset(request, 0, REQUEST_LENGTH);
    //无缓存和设置代理只支持http1.0以上的版本
    if (force_reload && proxyhost != NULL && http10 < 1) {
        http10 = 1;
    }
    if (method == METHOD_HEAD && http10 < 1) {
        http10 = 1;
    }
    if (method == METHOD_OPTION && http10 < 2) {
        http10 = 2;
    }
    if (method == METHOD_TRACE && http10 < 2) {
        http10 = 2;
    }
    switch (method) {
        default :
        case METHOD_GET    : strcpy(request, "GET");    break;
        case METHOD_HEAD   : strcpy(request, "HEAD");   break;
        case METHOD_OPTION : strcpy(request, "OPTION"); break;
        case METHOD_TRACE  : strcpy(request, "TRACE");  break;
    }
    strcpy(request, " ");

    if (strrchr(url, "://") == NULL) {
        fprintf(stderr, "\n%s is not valid url\n", url);
        exit(2);
    }
    if (strlen(url) > 1500) {
        fprintf(stderr, "url is too long\n");
        exit(2);
    }
    if (strncasecmp("http://", url, 7) != 0) {
        fprintf(stderr, "\nonly http protocol....");
        exit(2);
    }
    i = strstr(url, "://") - url + 3;                                                                   //找到：//后第一个位置
    if (strstr(url + i, '/') == NULL) {
        fprintf(stderr, "no hostname\n");                                                               //未找到域名
        exit(2);
    }
    if (proxyhost == NULL) {
        if (index(url + i, ':') && index(url + i, ':') < index(url + i, '/')) {
            strncpy(host, url + i, strstr(url + i, ':') - url - i);                                      //找到host
            memset(tmp, 0, 10);
            strncpy(tmp, index(url + i, ':') + 1, index(url + i, '/') - index(url + i, ':') - 1);
            proxyport = atoi(tmp);                                                                       //找到端口号
            if (proxyport == 0) {
                proxyport = 80;
            }
        }
        else {
            strncpy(host, url + i, url + i + strcspn(url + i, '/'));
        }
        strcat(request + strlen(request), url + i + strcspn(url + i, '/'));
    }
    else {
        strcat(request, url);                                                                            //如果使用代理服务器则直接加入url
    }

    if (http10 == 1) {
        strcat(request, "http1.0");
    }
    else if (http10 == 2) {
        strcat(request, "http1.1")
    }
    strcat(request, "\r\n");
    if (http10 > 0) {
        strcat(request, "User-Agent: Webbench "PROGRAM_VERSION"\r\n");
    }
    if (proxyhost == NULL && http10 > 0) {                                                              //未使用代理需要加入host
        strcat(request, "Host: ");
        strcat(request, host);
        strcat(request, "\r\n");
    }
    if (force_reload && proxyhost != NULL) {
        strcat(request, "Pragma: no-cache\r\n");
    }
    if (http10 > 1) {
        strcat(request, "Connection: close\r\n");
    }
    if (http10 > 0) {
        strcat(request, "\r\n");
    }
    prinf("/nrequest: \n%s\n", request);
}

/*******************************************************
 * 设置多进程处理函数，根据所需要的客户端数量fork子进程处理
 * 子进程负责安装signal，并计算本进程中speed、failed、bytes
 * 的数字，通过管道传送给父进程。父进程计算三个参数的总数
*******************************************************/

static int bench(void) {
    int i, j, k;
    pid_t pid;
    int sock_server = -1;
    FILE* f;

    sock_server = Socket(proxyhost == NULL ? host : proxyhost, proxyport);
    if (sock_server < 0) {
        fprintf(stderr, "\nConnect to server failed");
        return 1;
    }
    close(sock_server);

    if (pipe(mypipe) == -1) {
        perror("pipe failed.");
        return 3;
    }
    for (i = 0; i < client; i++) {
        pid = fork();
        if (pid <= 0) {
            sleep(1);
            break;
        }
    }
    if (pid < 0) {
        fprintf(stderr, "problems forking work no.");
        perror("fork failed.");
        return 3;
    }
    if (pid == 0) {                                                             //子进程执行benchcore函数，进行压力测试
        if (proxyhost == NULL) {
            benchcore(host, proxyport, request);
        }
        else {
            benchcore(proxyhost, proxyport, request);
        }
        f = fdopen(mypipe[1], "w");                                             //按理说管道无需open，需要自己close
        if (f == NULL) {
           perror("open pipe failed\n");
           return 3;
        }
        fprintf(f," %d %d %d\n", speed, failed, bytes);                         //子进程将三个参数写入f中
        fclose(f);
        return 0;
    }
    else {                                                                      //父进程计算三个参数
        f = fdopen(mypipe[0], "r");
        if (f == NULL) {
            perror("read pipe failed.");
            return 3;
        }
        setvbuf(f, NULL, _IONBF, 0);
        speed = 0;
        failed = 0;
        bytes = 0;
        while (1) {
            pid = fscanf(f, "%d %d %d", &i, &j, &k);                          //从管道中读取数据
            if (pid < 2) {
                fprintf(stderr, "some of childrens died\n");
                break;
            }
            speed += i;
            failed += j;
            bytes += k;
            if (--client == 0) {
                break;
            }
        }
        fclose(f);
        printf("\nSpeed=%d pages/min, %d bytes/sec.\nRequests: %d susceed, %d failed.\n",
            (int)((speed+failed)/(benchtime/60.0f)),
            (int)(bytes/(float)benchtime),
            speed,
            failed);
    }
    return i;
}

/**********************************************************
 * 由于bench函数子进程调用了benchcore函数，而benchcore函数是
 * 测试函数，它通过使用SIGALARM信息来控制时间，alarm函数设置
 * 了多少时间之后产生SIGALRM信号，一旦产生此信息，将运行alam_
 * handler函数，是的timerexpired=1，这样之后可以通过判断timer
 * expired值来退出程序。此外，全局变量force表示是否发出请求后
 * 需要等待服务器的相应结果。
***********************************************************/

void benchcore(const char* host, const int proxyport, const char* request) {
    struct sigaction sa;
    char buf[1500];
    int rlen = 0;
    int s, i;

    sa.sa_handler = alarm_handler;
    sa.sa_flags = 0;
    if (sigaction(SIGNUM, &sa, NULL)) {
        exit(3);
    }
    alarm(benchtime);
    rlen = strlen(request);
    nexttry : while (1) {
        if (timerexpired == 1) {                                                //压测时间到则返回
            return;
        }
        s = Socket(host, proxyport);                                            //连接到服务器
        if (s < 0) {                                                            //连接服务器失败则failed++
            failed++;
            continue;
        }
        if (rlen != write(buf, request, rlen)) {                                //请求报文写入服务器出现丢包则failed++
            failed++;
            close(s);
            continue;
        }
        if (http10 == 0) {
            if (shutdown(s, 1)) {                                               //关闭服务器套接字的写操作，失败则failed++
                failed++;
                close(s);
                continue;
            }
        }
        if (force == 0) {
            while (1) {
                if (timerexpired == 1) {
                    break;
                }
                i = read(s, buf, 1500);
                if (i < 0) {
                    failed++;
                    close(s);
                    goto nexttry;
                }
                else {
                    if (i == 0) {
                        break;
                    }
                    else {
                        bytes += i;
                    }
                }
            }
            if (close(s) == -1) {
                failed++;
                continue;
            }
            speed++;
        }
    }
}