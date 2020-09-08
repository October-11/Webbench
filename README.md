# WebBench源码解析

压力测试工具，分析对比两个网站的性能

​                           ![img](https://img-blog.csdn.net/20150418152758709?watermark/2/text/aHR0cDovL2Jsb2cuY3Nkbi5uZXQvamNqYzkxOA==/font/5a6L5L2T/fontsize/400/fill/I0JBQkFCMA==/dissolve/70/gravity/Center)



***

## **1.主要函数**

**alarm_handler** 信号处理函数，时钟结束时进行调用。

**usage** 输出 webbench 命令用法

**main** 提供程序入口...

**build_request** 构造 HTTP 请求

**bench** 派生子进程，父子进程管道通信最后输出计算结果。

**benchcore** 每个子进程的实际发起请求函数。

***

## 2.主要流程

程序执行的主要流程：

1. 解析命令行参数，根据命令行指定参数设定变量，可以认为是初始化配置。

   2.根据指定的配置构造 HTTP 请求报文格式。

   3.开始执行 bench 函数，先进行一次 socket 连接建立与断开，测试是否可以正常访问。

   4.建立管道，派生根据指定进程数派生子进程。

   5.每个子进程调用 benchcore 函数，先通过 sigaction 安装信号，用 alarm 设置闹钟函数，接着不断建立 socket 进行通信，与服务器交互数据，直到收到信号结束访问测试。子进程将访问测试结果写进管道。

   6.父进程读取管道数据，汇总子进程信息，收到所有子进程消息后，输出汇总信息，结束。

***

## 3.功能函数

### inet_ntoa() 和 inet_addr()



**inet_ntoa():**

- 函数原型：

```c
char *inet_ntoa(struct in_addr);1
```

- 参数：in_addr是一个结构体，用来表示一个32位的IPV4地址。

```c
  struct in_addr{
           in_addr_t s_addr;
       }123
```

- 返回值：返回点分十进制的字符串在静态内存中的指针。

  > **点分十进制：**
  >
  > - **全称为点分（点式）十进制表示法，是IPV4的IP地址标识方法。**
  > - **IPV4中用4个字节表示一个IP地址，每个字节按照十进制表示为0~255。**
  > - **点分十进制就是用4个从0~255的数字，来表示一个IP地址。**
  > - **例如：192.168.1.246**

- 头文件：`<arpa/inet.h>`

- 别称：IP地址转换函数。

- 功能：将网络字节序IP转化成点分十进制IP

  > **网络字节序：网络字节序是TCP/IP中规定好的一种数据表示格式，它与具体的CPU类型、操作系统等无关，从而可以保证数据在不同主机之间传输时能够被正确解释。网络字节顺序采用big endian（大端）排序方式。**



**inet_addr()**：

- 简介：
  inet_addr方法可以转化字符串，主要用来将一个十进制的数转化为二进制的数，用途多余IPV4的IP转化。
- 函数原型：

```
 in_addr_t inet_addr(const char* cp);
1
```

- 参数：字符串，一个点分十进制的IP地址。
- 返回值：
  若字符串有效，则将字符串转换为32位二进制网络字节序的IPV4地址；否则，为INADDR_NONE
- 头文件：`<arpa/inet.h>`
- 别称：IP地址转化函数。
- 功能：将一个点分十进制的IP转换成一个长整数型（u_long类型）。

***

 

### gethostbyname()

**gethostbyname()函数主要作用：用域名或者主机名获取地址，操作系统提供的库函数。**

头文件

```c
  #include <netdb.h>
  #include <sys/socket.h>
```

   函数原型

```
  struct hostent *gethostbyname(const char *name);
```

  这个函数的传入值是域名或者主机名，例如"www.google.cn"等等。传出值，是一个hostent的结构。如果函数调用失败，将返回NULL。

  这个函数的传入值是域名或者主机名，例如"www.google.cn"等等。传出值，是一个hostent的结构。如果函数调用失败，将返回NULL。

   返回hostent结构体类型指针

```c
  struct hostent
  {
    char  *h_name;        
    char  **h_aliases;
    int   h_addrtype;
    int   h_length;
    char  **h_addr_list;
    #define h_addr h_addr_list[0]
  };
```

  **hostent->h_name**
  表示的是主机的规范名。例如www.google.com的规范名其实是www.l.google.com。

  **hostent->h_aliases**
   表示的是主机的别名.www.google.com就是google他自己的别名。有的时候，有的主机可能有好几个别名，这些，其实都是为了易于用户记忆而为自己的网站多取的名字。

  **hostent->h_addrtype**   
  表示的是主机ip地址的类型，到底是ipv4(AF_INET)，还是pv6(AF_INET6)

  **hostent->h_length**    
  表示的是主机ip地址的长度

  **hostent->h_addr_lisst**
  表示的是主机的ip地址，注意，这个是以网络字节序存储的。千万不要直接用printf带%s参数来打这个东西，会有问题的哇。所以到真正需要打印出这个IP的话，需要调用inet_ntop()。

***



### connect()

定义函数：

```c
int connect (int sockfd, struct sockaddr * serv_addr, int addrlen);
```

connect函数通常用于客户端建立tcp连接。

connect函数通常用于客户端建立tcp连接。

**参数：**
sockfd：标识一个套接字。
serv_addr：套接字s想要连接的主机地址和端口号。
addrlen：name缓冲区的长度。

 **返回值：**
成功则返回0，失败返回-1，错误原因存于errno中。

 **错误代码：** 
EBADF 参数sockfd 非合法socket处理代码
EFAULT 参数serv_addr指针指向无法存取的内存空间
ENOTSOCK 参数sockfd为一文件描述词，非socket。
EISCONN 参数sockfd的socket已是连线状态
ECONNREFUSED 连线要求被server端拒绝。
ETIMEDOUT 企图连线的操作超过限定时间仍未有响应。
ENETUNREACH 无法传送数据包至指定的主机。
EAFNOSUPPORT sockaddr结构的sa_family不正确。
EALREADY socket为不可阻塞且先前的连线操作还未完成。

***

### getopt()

函数功能：用来解析命令行参数，参数argc和argv分别代表参数个数和内容，跟main（）函数里的命令行参数一样

**函数所在头文件**：

```c
#include<unistd.h>
```

**函数原型定义**：

```c
int getopt(int argc, char* const argv[ ], const char *optstring )
```

**参数optstring**: 为选项字符串，告知getopt可以处理那个选项以及哪个选项需要参数，如果选项字符串里的字母后接着冒号：“：”，则表示还有相关的参数，全域变量optarg即会指向此额外参数，如果在处理期间遇到了不符合optstring指定的其他选项，getopt()将会显示一个错误消息，并将全局域变量设置为“？”字符，将全局域变量opterr设置为0则将不会打印出错信息。

extern char* optarg;

extern int optind, opterr, optopt;

**参数optarg**：指向当前选项参数的指针

**参数optind**：再次调用getopt时的下一个argv指针索引

**参数optopt**：表示最后一个未知选项

**参数optstring**: 比如getopt(argc, argv, "td:ch:q::")

1. 单个字符，表示选项，这里一共有t、d、c、h、q五个选项

2. 单个字符后接一个冒号“：”表示该选项后必须跟一个参数，参数紧跟在选项后或者以空格隔开

3. 单个字符后跟两个冒号，表示该选项后可以跟一个参数，也可以不跟，如果后边跟一个参数，参数必须紧跟在选项后不能以空格隔开。

```c
char*optstring = “ab:c::”;
单个字符a         表示选项a没有参数            格式：-a即可，不加参数
单字符加冒号b:     表示选项b有且必须加参数      格式：-b 100或-b100,但-b=100错
单字符加2冒号c::   表示选项c可以有，也可以无     格式：-c200，其它格式错误
```

**返回值：**

1. 如getopt_long 返回-1，表示argv[]中的所有选项被解析出。
2. 如果成功找到了选项，getopt()会返回选项字符。

   3.如果遇到一个缺少参数的选项字符，返回值就要取决于optstring的第一个字符了：如果是’:’，则返回’:’，否则返回’?’

***

**例子：**

```c
#include<stdio.h>
#include<unistd.h>
#include<getopt.h>
int main(intargc, char *argv[])
{
    int opt;
    char *string = "a::b:c:d";
    while ((opt = getopt(argc, argv, string))!= -1)
    {  
        printf("opt = %c\t\t", opt);
        printf("optarg = %s\t\t",optarg);
        printf("optind = %d\t\t",optind);
        printf("argv[optind] = %s\n",argv[optind]);
    }  
}
```

**编译上述程序并执行结果：**

**输入选项及参数正确的情况**

```
dzlab:~/test/test#./opt -a100 -b 200 -c 300 -d
opt = a         optarg = 100            optind = 2              argv[optind] = -b
opt = b         optarg = 200            optind = 4              argv[optind] = -c
opt = c         optarg = 300            optind = 6              argv[optind] = -d
opt = d         optarg = (null)         optind = 7              argv[optind] = (null)
```

**或者这样的选项格式(注意区别)：**

```
dzlab:~/test/test#./opt -a100 -b200 -c300 -d 
opt = a         optarg = 100            optind = 2              argv[optind] = -b200
opt = b         optarg = 200            optind = 3              argv[optind] = -c300
opt = c         optarg = 300            optind = 4              argv[optind] = -d
opt = d         optarg = (null)         optind = 5              argv[optind] = (null)
```

**选项a是可选参数，这里不带参数也是正确的**

```
dzlab:~/test/test#./opt -a -b 200 -c 300 -d   
opt = a         optarg = (null)         optind = 2              argv[optind] = -b
opt = b         optarg = 200            optind = 4              argv[optind] = -c
opt = c         optarg = 300            optind = 6              argv[optind] = -d
opt = d         optarg = (null)         optind = 7              argv[optind] = (null)
```

**输入选项参数错误的情况**

```
dzlab:~/test/test#./opt -a 100 -b 200 -c 300 -d
opt = a         optarg = (null)         optind = 2              argv[optind] = 100
opt = b         optarg = 200            optind = 5              argv[optind] = -c
opt = c         optarg = 300            optind = 7              argv[optind] = -d
opt = d         optarg = (null)         optind = 8              argv[optind] = (null)
```

***



### getopt_long()

解析命令行选项参数

**头文件：**

```c
#include <getopt.h>
```

**函数原型：**

```c
int getopt_long(int argc, char* constargv[]， 
					const char* optstring, 
					const struct option* longopts, 
					int* longindex)
//longopts    指明了长参数的名称和属性
//longindex   如果longindex非空，它指向的变量将记录当前找到参数符合longopts里的第几个元素的描述，即是longopts的下标值
```

**参数解析：**

1. argc、argv直接从main函数中获取。
2. opting是选项参数组成的字符串，由下列元素组成：
   （1）单个字符，表示选项，
   （2）单个字符后接一个冒号：表示该选项后必须跟一个参数。参数紧跟在选项后或者以空格隔开。该参数的指针赋给optarg。
   （3）单个字符后跟两个冒号，表示该选项后可以有参数也可以没有参数。如果有参数，参数必须紧跟在选项后不能以空格隔开。该参数的指针赋给optarg。（这个特性是GNU的扩张）。
3. struct option结构体

```c
struct option {
const char *name; //name表示的是长参数名
int has_arg；
//has_arg有3个值，no_argument(或者是0)，表示该参数后面不跟参数值
// required_argument(或者是1),表示该参数后面一定要跟个参数值
// optional_argument(或者是2),表示该参数后面可以跟，也可以不跟参数值
int *flag;    
//用来决定，getopt_long()的返回值到底是什么。如果这个指针为NULL，那么getopt_long()返回该结构val字段中的数值。如果该指针不为NULL，getopt_long()会使得它所指向的变量中填入val字段中的数值，并且getopt_long()返回0。如果flag不是NULL，但未发现长选项，那么它所指向的变量的数值不变。
int val;    
//和flag联合决定返回值 这个值是发现了长选项时的返回值，或者flag不是 NULL时载入*flag中的值。典型情况下，若flag不是NULL，那么val是个真／假值，譬如1 或0；另一方面，如 果flag是NULL，那么val通常是字符常量，若长选项与短选项一致，那么该字符常量应该与optstring中出现的这个选项的参数相同。
12345678910
```

**返回值：**

1. 如getopt_long 返回-1，表示argv[]中的所有选项被解析出。
2. 如果成功找到了选项，getopt()会返回选项字符。

   3.如果遇到一个缺少参数的选项字符，返回值就要取决于optstring的第一个字符了：如果是’:’，则返回’:’，否则返回’?’

```
对于短选项，返回值同getopt函数；对于长选项，如果flag是NULL，返回val，否则返回0；对于错误情况返回值同getopt函数
```

**内部定义的一些全局变量：**
extern char* optarg;
extern int optind, opterr, optopt;
参数optarg：指向当前选项参数的指针
参数optind：再次调用getopt时的下一个argv指针索引
参数optopt：表示最后一个未知选项

***

**例子：**

```c
int main(intargc, char *argv[])
{
    int opt;
    int digit_optind = 0;
    int option_index = 0;
    char *string = "a::b:c:d";
    static struct option long_options[] =
    {  
        {"reqarg", required_argument,NULL, 'r'},
        {"optarg", optional_argument,NULL, 'o'},
        {"noarg",  no_argument,         NULL,'n'},
        {NULL,     0,                      NULL, 0},
    }; 
    while((opt =getopt_long_only(argc,argv,string,long_options,&option_index))!= -1)
    {  
        printf("opt = %c\t\t", opt);
        printf("optarg = %s\t\t",optarg);
        printf("optind = %d\t\t",optind);
        printf("argv[optind] =%s\t\t", argv[optind]);
        printf("option_index = %d\n",option_index);
```

编译上述程序并执行结果：

**正确输入长选项的情况**

```
dzlab:~/test/test#./long --reqarg 100 --optarg=200 --noarg
opt = r optarg =100     optind = 3   argv[optind] = --optarg=200  option_index = 0
opt = o optarg =200     optind = 4   argv[optind] = --noarg        option_index = 1
opt = n optarg =(null) optind = 5    argv[optind] =(null)          option_index = 2
```

**或者这种方式：**

```
dzlab:~/test/test#./long –reqarg=100 --optarg=200 --noarg
opt = r optarg =100     optind = 2   argv[optind] = --optarg=200  option_index = 0
opt = o optarg =200     optind = 3   argv[optind] = --noarg        option_index = 1
opt = n optarg =(null) optind = 4    argv[optind] =(null)          option_index = 2
```

**可选选项可以不给参数**

```
dzlab:~/test/test#./long --reqarg 100 --optarg --noarg   
opt = r optarg =100     optind = 3     argv[optind] = --optarg option_index = 0
opt = o optarg =(null) optind = 4      argv[optind] =--noarg   option_index = 1
opt = n optarg =(null) optind = 5      argv[optind] =(null)     option_index = 2
```

**输入长选项错误的情况**

```
dzlab:~/test/test#./long --reqarg 100 --optarg 200 --noarg 
opt = r optarg =100     optind = 3     argv[optind] = --optarg  option_index= 0
opt = o optarg =(null) optind = 4      argv[optind] =200        option_index = 1
opt = n optarg =(null) optind = 6      argv[optind] =(null)     option_index = 2
```

这时，虽然没有报错，但是第二项中 optarg 参数没有正确解析出来(格式应该是 —optarg=200)

**必须指定参数的选项，如果不给参数，同样解析错误如下：**

```
dzlab:~/test/test#./long --reqarg --optarg=200 --noarg    
opt = r optarg =--optarg=200  optind = 3 argv[optind] =--noarg  option_index = 0
opt = n optarg =(null)         optind = 4 argv[optind] =(null)    option_index = 2
```

***



### strrchr()

**原型：**

```c
char* strrchr(const char* str, char c)
```

**功能：**

从str的右侧开始查找字符c首次出现的位置

**返回值：**

如果查找到字符，则返回这个位置的地址

如果没找到，则返回null

**例子：**

```c++
#include <string.h>
#include <iostream.h>
void main(void)
{
    char sStr1[100];
    sStr1[0] = '\0';
    strcpy(sStr1,"Golden Global View");
    char *p = strrchr(sStr1,'i'); //从后往前查找'i'
    cout<<(p==NULL?"NULL":p)<<endl;

```

***



### strncasecmp()

定义

```c
int strncasecmp(const char *s1, const char *s2, size_t n);
```

描述

strncasecmp()用来比较参数s1 和s2 字符串前n个字符，比较时会自动忽略大小写的差异。

若参数s1 和s2 字符串相同则返回0。s1 若大于s2 则返回大于0 的值，s1 若小于s2 则返回小于0 的值。

```c
#include <string.h>
int main(){
    char *a = "aBcDeF";
    char *b = "AbCdEf";
    if(!strncasecmp(a, b, 3))
    printf("%s =%s\n", a, b);
    return 0;
}
```

***



### index()

**函数定义**：

```c
char *index(const char *s, int c);
```

**头文件**：  

```c
#include strings.h
```

**函数说明**：

```
index()用来找出参数s 字符串中第一个出现的参数c地址，然后将该字符出现的地址返回。字符串结束字符(NULL)也视为字符串一部分。
```

**返回值**：

```
如果找到指定的字符则返回该字符所在地址，否则返回NULL
```

***



### strcspn()

C 库函数 **size_t strcspn(const char \*str1, const char \*str2)** 检索字符串 **str1** 开头连续有几个字符都不含字符串 **str2** 中的字符。

**声明**

下面是 strcspn() 函数的声明。

```
size_t strcspn(const char *str1, const char *str2)
```

**参数**

- **str1** -- 要被检索的 C 字符串。
- **str2** -- 该字符串包含了要在 str1 中进行匹配的字符列表。

**返回值**

该函数返回 str1 开头连续都不含字符串 str2 中字符的字符数。

**实例**

下面的实例演示了 strcspn() 函数的用法。

```c
#include <stdio.h>
#include <string.h>


int main ()
{
   int len;
   const char str1[] = "ABCDEF4960910";
   const char str2[] = "013";

   len = strcspn(str1, str2);

   printf("第一个匹配的字符是在 %d\n", len + 1);
   
   return(0);
}
```

让我们编译并运行上面的程序，这将产生以下结果：

```c
第一个匹配的字符是在 10
```

***



### fdopen()

头文件：

```
#include <stdio.h>
```

fopen()是一个常用的函数，用来以指定的方式打开文件，其原型为：


```
FILE* fdopen(const char* path, const char* mode);
```

fdopen 函数用于在一个已经打开的文件描述符上打开一个流，其第1个参数表示一个已经打开的文件描述符，第2个参数type的意义和fopen函数的第2个参数一 样。只有一点不同的是，由于文件已经被打开，所以fdopen函数不会创建文件，而且也不会将文件截短为0，这一点要特别注意。

**函数说明**：fdopen()会将参数fildes 的文件描述词, 转换为对应的文件指针后返回.

**参数mode** 字符串则代表着文件指针的流形态, 此形态必须和原先文件描述词读写模式相同. 关于mode 字符串格式请参考fopen().

**返回值**：转换成功时返回指向该流的文件指针. 失败则返回NULL, 并把错误代码存在errno 中.

***



### setvbuf()

**描述**

将缓冲与流相关

C 库函数 **int setvbuf(FILE \*stream, char \*buffer, int mode, size_t size)** 定义流 stream 应如何缓冲。

**声明**

下面是 setvbuf() 函数的声明。

```
int setvbuf(FILE *stream, char *buffer, int mode, size_t size)
```

**参数**

- **stream** -- 这是指向 FILE 对象的指针，该 FILE 对象标识了一个打开的流。
- **buffer** -- 这是分配给用户的缓冲。如果设置为 NULL，该函数会自动分配一个指定大小的缓冲。
- **mode** -- 这指定了文件缓冲的模式：

| 模式   | 描述                                                         |
| :----- | :----------------------------------------------------------- |
| _IOFBF | **全缓冲**：对于输出，数据在缓冲填满时被一次性写入。对于输入，缓冲会在请求输入且缓冲为空时被填充。 |
| _IOLBF | **行缓冲**：对于输出，数据在遇到换行符或者在缓冲填满时被写入，具体视情况而定。对于输入，缓冲会在请求输入且缓冲为空时被填充，直到遇到下一个换行符。 |
| _IONBF | **无缓冲**：不使用缓冲。每个 I/O 操作都被即时写入。buffer 和 size 参数被忽略。 |

- **size** --这是缓冲的大小，以字节为单位。

**返回值**

如果成功，则该函数返回 0，否则返回非零值。

***



### fscanf()

**描述**

C 库函数 **int fscanf(FILE \*stream, const char \*format, ...)** 从流 stream 读取格式化输入。

**声明**

下面是 fscanf() 函数的声明。

```
int fscanf(FILE *stream, const char *format, ...)
```

**参数**

- **stream** -- 这是指向 FILE 对象的指针，该 FILE 对象标识了流。
- **format** -- 这是 C 字符串，包含了以下各项中的一个或多个：*空格字符、非空格字符* 和 *format 说明符*。
  format 说明符形式为 **[=%[\*][width][modifiers]type=]**，具体讲解如下：

| 参数      | 描述                                                         |
| :-------- | :----------------------------------------------------------- |
| *         | 这是一个可选的星号，表示数据是从流 stream 中读取的，但是可以被忽视，即它不存储在对应的参数中。 |
| width     | 这指定了在当前读取操作中读取的最大字符数。                   |
| modifiers | 为对应的附加参数所指向的数据指定一个不同于整型（针对 d、i 和 n）、无符号整型（针对 o、u 和 x）或浮点型（针对 e、f 和 g）的大小： h ：短整型（针对 d、i 和 n），或无符号短整型（针对 o、u 和 x） l ：长整型（针对 d、i 和 n），或无符号长整型（针对 o、u 和 x），或双精度型（针对 e、f 和 g） L ：长双精度型（针对 e、f 和 g） |
| type      | 一个字符，指定了要被读取的数据类型以及数据读取方式。具体参见下一个表格。 |

**fscanf 类型说明符：**

| 类型      | 合格的输入                                                   | 参数的类型     |
| :-------- | :----------------------------------------------------------- | :------------- |
| c         | 单个字符：读取下一个字符。如果指定了一个不为 1 的宽度 width，函数会读取 width 个字符，并通过参数传递，把它们存储在数组中连续位置。在末尾不会追加空字符。 | char *         |
| d         | 十进制整数：数字前面的 + 或 - 号是可选的。                   | int *          |
| e,E,f,g,G | 浮点数：包含了一个小数点、一个可选的前置符号 + 或 -、一个可选的后置字符 e 或 E，以及一个十进制数字。两个有效的实例 -732.103 和 7.12e4 | float *        |
| o         | 八进制整数。                                                 | int *          |
| s         | 字符串。这将读取连续字符，直到遇到一个空格字符（空格字符可以是空白、换行和制表符）。 | char *         |
| u         | 无符号的十进制整数。                                         | unsigned int * |
| x,X       | 十六进制整数。                                               | int *          |

- **附加参数** -- 根据不同的 format 字符串，函数可能需要一系列的附加参数，每个参数包含了一个要被插入的值，替换了 format 参数中指定的每个 % 标签。参数的个数应与 % 标签的个数相同。

**返回值**

如果成功，该函数返回成功匹配和赋值的个数。如果到达文件末尾或发生读错误，则返回 EOF。

***



### sigaction()

**函数原型**：sigaction函数的功能是检查或修改与指定信号相关联的处理动作（可同时两种操作）

```c
int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);
```

signum参数指出要捕获的信号类型，act参数指定新的信号处理方式，oldact参数输出先前信号的处理方式（如果不为NULL的话）。

**struct sigaction结构体介绍****

```c
struct sigaction {
    void (* sa_handler)(int);
    void (* sa_sigaction)(int, siginfo_t *, void *);
    sigset_t sa_mask;
    int sa_flags;
    void (* sa_restorer)(void);
}
```

- sa_handler此参数和signal()的参数handler相同，代表新的信号处理函数
- sa_mask 用来设置在处理该信号时暂时将sa_mask 指定的信号集搁置
- sa_flags 用来设置信号处理的其他相关操作，下列的数值可用。 
- SA_RESETHAND：当调用信号处理函数时，将信号的处理函数重置为缺省值SIG_DFL
- SA_RESTART：如果信号中断了进程的某个系统调用，则系统自动启动该系统调用
- SA_NODEFER ：一般情况下， 当信号处理函数运行时，内核将阻塞该给定信号。但是如果设置了 SA_NODEFER标记， 那么在该信号处理函数运行时，内核将不会阻塞该信号

**返回值：**

sigaction成功则返回0，失败则返回-1，超时会产生信号SIGALRM，用sa指定函数处理

***



### alarm()

**alarm()函数说明**

1.引用头文件：

```
#include <unistd.h>;
```

2.函数标准式：

```
unsigned int alarm(unsigned int seconds);
```

3.功能与作用：alarm()函数的主要功能是设置信号传送闹钟，即用来设置信号SIGALRM在经过参数seconds秒数后发送给目前的进程。如果未设置信号SIGALARM的处理函数，那么alarm()默认处理终止进程。

4.函数返回值：如果在seconds秒内再次调用了alarm函数设置了新的闹钟，则后面定时器的设置将覆盖前面的设置，即之前设置的秒数被新的闹钟时间取代；当参数seconds为0时，之前设置的定时器闹钟将被取消，并将剩下的时间返回。

**alarm()测试1.1**

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
  
void sig_handler(int num)
{
    printf("receive the signal %d.\n", num);
}
  
int main()
{
    signal(SIGALRM, sig_handler); //SIGALRM是在定时器终止时发送给进程的信号
    alarm(2);
    pause();//pause()函数使该进程暂停让出CPU
    exit(0);
}
```

　　运行结果：两秒钟后输出 

![img](https://img2018.cnblogs.com/blog/1169746/201810/1169746-20181015111256618-7689955.png)

***



### close()和shutdown()函数

**1.close()函数**

```cpp
#include<unistd.h>
int close(int sockfd);     //返回成功为0，出错为-1.
```

  close 一个套接字的默认行为是把套接字标记为已关闭，然后立即返回到调用进程，该套接字描述符不能再由调用进程使用，也就是说它不能再作为read或write的第一个参数，然而TCP将尝试发送已排队等待发送到对端的任何数据，发送完毕后发生的是正常的TCP连接终止序列。

  在多进程并发服务器中，父子进程共享着套接字，套接字描述符引用计数记录着共享着的进程个数，当父进程或某一子进程close掉套接字时，描述符引用计数会相应的减一，当引用计数仍大于零时，这个close调用就不会引发TCP的四路握手断连过程。

**2.shutdown()函数**

```cpp
#include<sys/socket.h>
int shutdown(int sockfd,int howto);  //返回成功为0，出错为-1.
```

  调用 close()/close[socket](http://c.biancheng.net/socket/)() 函数意味着完全断开连接，即不能发送数据也不能接收数据，这种“生硬”的方式有时候会显得不太“优雅”。

![close()/closesocket() 断开连接](http://c.biancheng.net/uploads/allimg/190219/1350141P8-0.jpg)
图1：close()/closesocket() 断开连接


上图演示了两台正在进行双向通信的主机。主机A发送完数据后，单方面调用 close()/closesocket() 断开连接，之后主机A、B都不能再接受对方传输的数据。实际上，是完全无法调用与数据收发有关的函数。

一般情况下这不会有问题，但有些特殊时刻，需要只断开一条数据传输通道，而保留另一条。

该函数的行为依赖于howto的值

- SHUT_RD：值为0，断开输入流（输入）。套接字无法接收数据（即使输入缓冲区收到数据也被抹去），无法调用输入相关函数。
- SHUT_WR：值为1，断开输出流（发送）。套接字无法发送数据，但如果输出缓冲区中还有未传输的数据，则将传递到目标主机。
- SHUT_RDWR：值为2，输入输出都关闭。同时断开 I/O 流。相当于分两次调用 shutdown()，其中一次以 SHUT_RD 为参数，另一次以 SHUT_WR 为参数。

  终止网络连接的通用方法是调用close函数。但使用shutdown能更好的控制断连过程（使用第二个参数）。

**3.两函数的区别
**  **close与shutdown的区别主要表现在**：
  close函数会关闭套接字ID，如果有其他的进程共享着这个套接字，那么它仍然是打开的，这个连接仍然可以用来读和写，并且有时候这是非常重要的 ，特别是对于多进程并发服务器来说。

  而shutdown会切断进程共享的套接字的所有连接，不管这个套接字的引用计数是否为零，那些试图读得进程将会接收到EOF标识，那些试图写的进程将会检测到SIGPIPE信号，同时可利用shutdown的第二个参数选择断连的方式。