#include <stdio.h>      // 标准输入输出库
#include <stdlib.h>     // 标准库，包含系统函数
#include <unistd.h>     // Unix标准函数定义
#include <sys/types.h>  // 数据类型
#include <sys/socket.h> // 套接字接口
#include <netinet/in.h> // Internet地址族
#include <strings.h>    // 字符串操作函数，如bzero
#include <string.h>     // 字符串操作函数，如strncpy
#include <arpa/inet.h>  // 提供inet_addr等函数
#include <errno.h>      // 错误号
#include <sys/wait.h>   // 进程控制
#include <pthread.h>    // POSIX线程库
#include <time.h>       // 时间函数

#define PORT 10005      // 服务器监听端口
#define MAX_CLIENTS 10  // 最大客户端连接数
#define MAXSIZE 1024   // 最大消息长度

// 聊天消息结构体，包含客户端名称和消息内容
typedef struct {
    char client_name[100];
    char message[1024];
} ChatMessage;

// 全局变量，存储客户端套接字描述符和客户端计数
int clients[MAX_CLIENTS];
int client_count = 0;
pthread_mutex_t lock; // 互斥锁，用于同步客户端数组

// 函数声明
int SendToClients(int exclude_sock, const char* client_name, const char* message);
void SaveChatHistory(const char* client_name, const char* message);

// 客户端处理函数
void *ClientHandler(void *socket_desc) {
    int sock = *(int*)socket_desc; // 强制转换void*为int类型
    ChatMessage chat_msg;
    int read_size;

    // 循环接收消息
    while ((read_size = recv(sock, &chat_msg, sizeof(chat_msg), 0)) > 0) {
        // 打印接收到的消息
        printf("%s: %s\n", chat_msg.client_name, chat_msg.message);

        // 检查是否为退出命令
        if (strcmp(chat_msg.message, "exit") == 0) {
            // 发送退出消息给其他客户端
            char exit_msg[1024];
            snprintf(exit_msg, sizeof(exit_msg), "%s has left the chat.", chat_msg.client_name);
            SendToClients(sock, chat_msg.client_name, exit_msg);

            // 从客户端数组中移除该客户端
            pthread_mutex_lock(&lock);
            for (int i = 0; i < client_count; i++) {
                if (clients[i] == sock) {
                    for (int j = i; j < client_count - 1; j++) {
                        clients[j] = clients[j + 1];
                    }
                    client_count--;
                    break;
                }
            }
            pthread_mutex_unlock(&lock);

            // 关闭套接字并释放内存
            close(sock);
            free(socket_desc);
            pthread_exit(NULL);
        } else {
            // 将消息转发给其他客户端
            SendToClients(sock, chat_msg.client_name, chat_msg.message);
        }

        // 清空消息缓冲区
        bzero(&chat_msg, sizeof(ChatMessage));
    }

    if (read_size == 0) {
        printf("Client %d disconnected.\n", sock);
    } else {
        perror("recv failed");
    }

    // 如果recv失败，再次检查是否需要发送退出消息
    if (read_size <= 0) {
        char exit_msg[1024];
        snprintf(exit_msg, sizeof(exit_msg), "%s has left the chat.", chat_msg.client_name);
        SendToClients(sock, chat_msg.client_name, exit_msg);
    }

    // 关闭套接字并释放内存
    close(sock);
    free(socket_desc);
    pthread_exit(NULL);
}

// 消息转发函数
int SendToClients(int exclude_sock, const char* client_name, const char* message) {
    for (int i = 0; i < client_count; i++) {
        if (clients[i] != exclude_sock) {
            ChatMessage chat_msg;
            strncpy(chat_msg.client_name, client_name, sizeof(chat_msg.client_name) - 1);
            chat_msg.client_name[sizeof(chat_msg.client_name) - 1] = '\0';
            strncpy(chat_msg.message, message, sizeof(chat_msg.message) - 1);
            chat_msg.message[sizeof(chat_msg.message) - 1] = '\0';
            send(clients[i], &chat_msg, sizeof(ChatMessage), 0);
        }
    }
    // 保存聊天历史
    SaveChatHistory(client_name, message);
    return 0;
}

// 保存聊天内容到文件
void SaveChatHistory(const char* client_name, const char* message) {
    FILE* file = fopen("chat_history.txt", "a"); // 以追加模式打开文件
    if (file) {
        time_t now = time(NULL); // 获取当前时间
        char* time_str = ctime(&now); // 将时间转换为字符串
        fprintf(file, "[%s] %s: %s\n", time_str, client_name, message); // 写入文件
        fclose(file); // 关闭文件
    }
}

int main() {
    int listenfd, *new_sock; // 监听套接字和新的套接字描述符
    struct sockaddr_in server, client; // 服务器和客户端地址结构体
    socklen_t sin_size; // 地址结构体大小
    pthread_t tid; // 线程ID

    // 初始化互斥锁
    pthread_mutex_init(&lock, NULL);
    sin_size = sizeof(struct sockaddr_in); // 设置地址结构体大小

    // 创建套接字
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // 设置服务器地址
    bzero(&server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = INADDR_ANY;


    // 绑定套接字
    if (bind(listenfd, (struct sockaddr *)&server, sizeof(server)) == -1) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // 监听连接
    if (listen(listenfd, 3) == -1) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", PORT); // 打印服务器监听信息

    while (1) {
        if ((new_sock = malloc(sizeof(int))) == NULL) { // 分配内存存储新的套接字描述符
            perror("malloc failed");
            exit(EXIT_FAILURE);
        }

        // 接受新的客户端连接
        if ((client_count < MAX_CLIENTS) && (*new_sock = accept(listenfd, (struct sockaddr *)&client, &sin_size)) > 0) {
            clients[client_count] = *new_sock; // 存储新的套接字描述符
            client_count++; // 增加客户端计数
            printf("New connection, socket fd is %d, client count is %d\n", *new_sock, client_count); // 打印新连接信息

            pthread_create(&tid, NULL, ClientHandler, (void *)new_sock); // 创建线程处理客户端
            pthread_detach(tid); // 释放线程资源
        } else {
            if (client_count >= MAX_CLIENTS) {
                printf("Server is full. No more clients can be added.\n"); // 服务器已满
            } else {
                perror("accept failed"); // 接受失败
            }
            free(new_sock); // 释放内存
        }
    }

    close(listenfd); // 关闭监听套接字
    return 0;
}
