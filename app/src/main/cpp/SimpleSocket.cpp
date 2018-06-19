//
// Created by 刘璟博 on 2018/6/13.
//
#include "com_liu_echo_EchoServerActivity.h"
#include "com_liu_echo_EchoClientActivity.h"
#include <stdio.h> // NULL
#include <stdarg.h> // va_list, vsnprintf
#include <errno.h> // errno
#include <string.h> // strerror_r, memset

// socket, bind, getsockname, listen, accept, recv, send, connect
#include <sys/types.h>
#include <sys/socket.h>

#include <sys/un.h> // sockaddr_un
#include <netinet/in.h> // htons, sockaddr_in
#include <arpa/inet.h> // inet_ntop
#include <unistd.h> // close, unlink
#include <stddef.h> // offsetof

// 最大数据缓冲区大小
#define MAX_BUFFER_SIZE 80

/**
 * TCP socket 服务开发的4个步骤 socket->bind->listen->accept
 * @param env
 * @param obj
 * @param port
 */
void Javatest_com_liu_echo_EchoServerActivity_nativeStartTcpServer
        (JNIEnv *env, jobject obj, jint port) {
    /*
     * 1. 用 socket 函数来创建Socket
     *      int socket(int domain, int type, int protocol);
     */
    int tcpSocket = socket(PF_INET, SOCK_STREAM, 0);

    /*
     * 2. 绑定 socket 到给定的端口
     */
    // socket 被绑定的地址协议
    struct sockaddr_in address;

    // 2.1 绑定 socket 地址
    memset(&address, 0, sizeof(address)); // 把 address 中的所有数据都设为 0.
    address.sin_family = PF_INET; // Internet第4版协议，使应用程序可以与网络上其他地方运行的应用程序进行通信

    // 2.2 绑定到所有地址 将IP地址绑定到默认网卡上
    address.sin_addr.s_addr = htonl(INADDR_ANY); // host to network long 将主机的无符号长整形数转换成网络字节顺序

    // 2.3 将端口转换为网络字节顺序
    address.sin_port = htons(port); // 函数全意 host to network short 将主机的无符号短整形数转换成网络字节顺序

    // 2.4 根据所给参数进行绑定进行绑定
    bind(tcpSocket, (struct sockaddr *) &address, sizeof(address));

    /*
     * 3. 获取当前 socket 绑定的 port
     */
    // 3.1 定义接受数据的参数
    unsigned short cPort = 0;
    struct sockaddr_in address0;
    socklen_t addressLength0 = sizeof(address0);
    // 3.2 将现有 socket 信息放入 当前 sockaddr_in 参数中
    /*
     * __socketcall int getsockname(int __fd, struct sockaddr* __addr, socklen_t* __addr_length);
     */
    getsockname(tcpSocket, (struct sockaddr *) &address0, &addressLength0);

    // 3.3 将端口转换为主机字节顺序得到 port
    cPort = ntohs(address0.sin_port); // network to host short 将一个无符号短整形数从网络字节顺序转换为主机字节顺序

    /*
     * 4 listen 函数把一个未连接的 socket 转换成一个被动 socket
     *     backlog: 指定保存挂起的输入连接的队列大小
     */
    listen(tcpSocket, 4);

    /*
     * 5 接受传入连接 accept 返回该连接实例交互时会用到的客户 socket 描述符
     */
    // 5.1 定义接受数据的参数
    struct sockaddr_in address1;  // 来自客户端的协议地址
    socklen_t addressLength1 = sizeof(address1);

    // 5.2 阻塞和等待进来的客户链接 并且 接受它
    int clientSocket = accept(tcpSocket, (struct sockaddr *) &address1, &addressLength1);

    /*
     * 6 接收并发送数据
     */
    char buffer[MAX_BUFFER_SIZE];
    ssize_t recvSize;
    ssize_t sentSize;

    // 接收并发送数据
    while (1) {
        // 从 socket 中接收
        /*
         * recv 函数
         *     socket descriptor: 指定接收数据的 socket 实例
         *     buffer pointer: 指向内存地址的指针，用于保存接收数据
         *     buffer length: 数据缓冲区大小
         *     flags: 指定接收所需的额外标志
         */
        recvSize = recv(clientSocket, buffer, MAX_BUFFER_SIZE - 1, 0);

        // 以 NULL 结尾缓冲区形成一个字符串
        buffer[recvSize] = NULL;

        if ((0 == recvSize) || (NULL != env->ExceptionOccurred())) {
            break;
        }

        // 发送给 socket
        /*
         * send 函数
         *     socket descriptor: 指定接收数据的 socket 实例
         *     buffer pointer: 指向内存地址的指针，用于保存接收数据
         *     buffer length: 数据缓冲区大小
         *     flags: 指定接收所需的额外标志
         */
        sentSize = send(clientSocket, buffer, (size_t) recvSize, 0);

        if ((0 == sentSize) || (NULL != env->ExceptionOccurred())) {
            break;
        }
    }

    // 关闭客户端
    close(clientSocket);

}

/**
 * TCP socket 客户端开发 socket->connect->(send/recv)
 * @param env
 * @param obj
 * @param ip
 * @param port
 * @param message
 */
void Javatest_com_liu_echo_EchoClientActivity_nativeStartTcpClient
        (JNIEnv *env, jobject obj, jstring ip, jint port, jstring message) {
    /*
     * 1 构造新的 TCP socket
     */
    int clientSocket = socket(PF_INET, SOCK_STREAM, 0);

    /*
     * 2 连接到 IP 地址和端口
     */
    // 2.1 以 C 字符串形式获取 IP 地址
    const char *ipAddress = env->GetStringUTFChars(ip, NULL);

    // 2.2 连接到给定的 IP 地址和端口号
    // 2.2.1 准备地址协议参数
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address)); // 初始化结构体 address 中所有参数为 0
    address.sin_family = PF_INET;

    // 2.2.2 将 IP 地址字符串转换为网络地址
    inet_aton(ipAddress, &(address.sin_addr));

    // 2.2.3 将端口号转换为网络字节顺序
    address.sin_port = htons(port);// host to network short

    // 2.2.4 进行连接
    connect(clientSocket, (const sockaddr *) &address, sizeof(address));

    // 2.3 释放已经用完的 IP 地址，
    env->ReleaseStringUTFChars(ip, ipAddress);

    /*
     * 3 发送消息
     */
    // 3.1 以 C 字符串的形式获取消息
    const char *messageText = env->GetStringUTFChars(message, NULL);

    // 3.2 获取消息大小
    jsize messageSize = env->GetStringUTFLength(message);

    // 3.3 发送消息
    /*
     * send 函数
     *     socket descriptor: 指定接收数据的 socket 实例
     *     buffer pointer: 指向内存地址的指针，用于保存接收数据
     *     buffer length: 数据缓冲区大小
     *     flags: 指定接收所需的额外标志
     */
    ssize_t sentSize = send(clientSocket, messageText, (size_t) messageSize, 0);

    // 3.4 释放已经用完的消息文本
    env->ReleaseStringUTFChars(message, messageText);

    /*
     * 4 从 socket 接收
     */
    // 4.1 创建消息缓冲区
    char buffer[MAX_BUFFER_SIZE];

    // 4.2 接收
    /*
     * recv 函数
     *     socket descriptor: 指定接收数据的 socket 实例
     *     buffer pointer: 指向内存地址的指针，用于保存接收数据
     *     buffer length: 数据缓冲区大小
     *     flags: 指定接收所需的额外标志
     */
    ssize_t recvSize = recv(clientSocket, buffer, MAX_BUFFER_SIZE - 1, 0);

    // 4.3 以 NULL 结尾缓冲区形成一个字符串
    buffer[recvSize] = NULL;

    // 5 关闭 socket
    close(clientSocket);
}