#include <stdio.h>      // 标准输入输出库
#include <stdlib.h>     // 标准库，包含系统函数
#include <unistd.h>     // Unix标准函数定义
#include <string.h>     // 字符串操作函数
#include <strings.h>    // 字符串操作函数，如bzero
#include <sys/types.h>  // 数据类型
#include <sys/socket.h> // 套接字接口
#include <netinet/in.h> // Internet地址族
#include <netdb.h>      // 网络数据库操作
#include <pthread.h>    // POSIX线程库

#define PORT 10005      // 服务器端口
#define MAXSIZE 4096    // 最大消息长度

// 聊天消息结构体，包含客户端名称和消息内容
typedef struct {
    char client_name[100];
    char message[1024];
} ChatMessage;

char sendbuf[MAXSIZE]; // 发送缓冲区
char name[100];        // 客户端名称
int fd;               // 套接字描述符

// 发送消息的函数
void SendMessage(int sock, const char* name, const char* message) {
    ChatMessage chat_msg;
    strncpy(chat_msg.client_name, name, sizeof(chat_msg.client_name) - 1); // 复制客户端名称
    chat_msg.client_name[sizeof(chat_msg.client_name) - 1] = '\0'; // 确保字符串以NULL结尾
    strncpy(chat_msg.message, message, sizeof(chat_msg.message) - 1); // 复制消息内容
    chat_msg.message[sizeof(chat_msg.message) - 1] = '\0'; // 确保字符串以NULL结尾
    send(sock, &chat_msg, sizeof(ChatMessage), 0); // 发送消息
}

// 用来接收消息的子线程函数
void *pthread_recv(void *ptr) {
	ChatMessage chat_msg;
	while (1) {
	bzero(&chat_msg, sizeof(ChatMessage)); // 清空消息结构体
	if (recv(fd, &chat_msg, sizeof(ChatMessage), 0) == -1) { // 接收消息
	printf("recv() error\n");
	exit(1);
	}
	printf("%s: %s\n", chat_msg.client_name, chat_msg.message); // 打印接收到的消息
        }
return NULL;
}

int main(int argc, char *argv[]) {
	struct hostent *he; // 网络地址结构体
	struct sockaddr_in server; // 服务器地址结构体
// 检查命令行参数
if (argc != 2) {
    printf("Usage: %s <IP Address>\n", argv[0]);
    exit(1);
}

// 获取主机信息
if ((he = gethostbyname(argv[1])) == NULL) {
    printf("gethostbyname() error\n");
    exit(1);
}

// 创建套接字
if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    printf("socket() error\n");
    exit(1);
}

// 设置服务器地址
bzero(&server, sizeof(server));
server.sin_family = AF_INET;
server.sin_port = htons(PORT);
bcopy((char *)he->h_addr, (char *)&server.sin_addr.s_addr, he->h_length);

// 连接到服务器
if (connect(fd, (struct sockaddr *)&server, sizeof(server)) == -1) {
    printf("connect() error\n");
    exit(1);
}

printf("Connected to server......\n"); // 打印连接成功信息
printf("                     [Welcome to the chat room]\n"); 
printf("Enter your name: ");
fgets(name, sizeof(name), stdin); // 输入客户端名称
name[strcspn(name, "\n")] = 0; // 去除换行符
printf("You have joined the chat！\n");
SendMessage(fd, name, "has joined the chat."); // 发送加入聊天室的消息

// 创建子线程接收消息
pthread_t tid;
pthread_create(&tid, NULL, pthread_recv, NULL);
pthread_detach(tid);

// 客户端输入循环

while (1) {
    bzero(sendbuf, MAXSIZE); // 清空发送缓冲区
    fgets(sendbuf, MAXSIZE, stdin); // 输入消息
    sendbuf[strcspn(sendbuf, "\n")] = 0; // 去除换行符
    // 检查退出命令
	if (strcmp(sendbuf, "exit") == 0) {
    printf("You have exited the chat room.\n"); // 打印退出信息
    SendMessage(fd, name, "exit"); // 发送退出命令和客户端名称
    close(fd); // 关闭套接字
    break; // 退出循环
}

    // 发送消息
    SendMessage(fd, name, sendbuf);
}
return 0;
}
