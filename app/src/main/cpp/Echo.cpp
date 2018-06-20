#include "com_liu_echo_EchoServerActivity.h"
#include "com_liu_echo_EchoClientActivity.h"
#include "com_liu_echo_LocalSocketActivity.h"
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

// 最大日志消息长度
#define MAX_LOG_MESSAGE_LENGTH 256

// 最大数据缓冲区大小
#define MAX_BUFFER_SIZE 80

/**
 * 将给定的消息记录到应用程序
 * @param env
 * @param obj
 * @param format
 * @param ...
 */
static void LogMessage(JNIEnv *env, jobject obj, const char *format, ...) {
    // 缓存日志方法ID
    static jmethodID methodID = NULL;

    // 如果方法ID未缓存
    if (methodID == NULL) {
        // 从对象类获取类
        jclass clazz = env->GetObjectClass(obj);
        // 从获取的类获取方法ID
        methodID = env->GetMethodID(clazz, "logMessage", "(Ljava/lang/String;)V");
        // 释放类的引用
        env->DeleteLocalRef(clazz);
    } else { // 找到方法
        // 格式化日志消息
        char buffer[MAX_LOG_MESSAGE_LENGTH];

        va_list ap; // 一个指向参数的指针
        va_start(ap, format); // 初始化ap, 指向参数format之后的参数的地址
        vsnprintf(buffer, MAX_LOG_MESSAGE_LENGTH, format, ap);
        va_end(ap);

        // 将缓冲区转换为Java字符串
        jstring message = env->NewStringUTF(buffer);

        // 如果字符串构造正确
        if (NULL != message) {
            // 记录消息
            env->CallVoidMethod(obj, methodID, message);

            // 释放消息引用
            env->DeleteLocalRef(message);
        }
    }

}

static void ThrowException(JNIEnv *env, const char *className, const char *message) {
    // 获取异常类
    jclass clazz = env->FindClass(className);

    // 如果异常类找到
    if (NULL != clazz) {
        // 抛出异常
        env->ThrowNew(clazz, message);

        // 释放原生类引用
        env->DeleteLocalRef(clazz);
    }
}

/**
 * 用给定异常类和基于错误号的错误消息抛出新异常
 * @param env
 * @param className
 * @param errnum
 */
static void ThrowErrnoException(JNIEnv *env, const char *className, int errnum) {

    char buffer[MAX_LOG_MESSAGE_LENGTH];

    // 获取错误号消息 返回 0 成功，返回 -1 失败
    if (-1 == strerror_r(errnum, buffer, MAX_LOG_MESSAGE_LENGTH)) {
        strerror_r(errno, buffer, MAX_LOG_MESSAGE_LENGTH);
    }

    // 抛出异常
    ThrowException(env, className, buffer);
}

static int NewTcpSocket(JNIEnv *env, jobject obj) {
    // 构造Socket
    LogMessage(env, obj, "Constructing a new TCP socket...");
    /*
     * 用 socket 函数来创建Socket
     *      int socket(int domain, int type, int protocol);
     *      Domain:指定产生的通信域，选择用到协议族。Android支持以下协议族
     *          > PF_LOCAL:主机内部通信协议族，运行在同一设备上
     *          > PF_INET:Internet 第 4 版协议族，进行网络通信
     *      Type:指通信的语义
     *          > SOCK_STREAM:提供使用 TCP 协议的面向连接的通信 Stream socket 类型
     *          > SOCK_DGRAM:提供使用 UDP 协议的无连接的通信 Datagram socket 类型
     *      Protocol:指定将会用到的协议。一般默认参数写 0 即可
     */
    int tcpSocket = socket(PF_INET, SOCK_STREAM, 0);

    // 检查 socket 构造是否正确
    if (-1 == tcpSocket) {
        // 抛出带错误号的异常
        ThrowErrnoException(env, "java/io/IOException", errno);
    }

    return tcpSocket;
}

/**
 * 将 socket 绑定到某一个端口号
 * @param env JNIEnv interface
 * @param obj jobject instance
 * @param sd socket 描述符来表示socket
 * @param port 端口号或者设置为 0 将随机分配第一个可用端口号
 */
static void BindSocketToPort(JNIEnv *env, jobject obj, int sd, unsigned short port) {
    /*
     * struct sockaddr_in {
     *     sa_family sin_family;
     *     unsigned short int sin_port;
     *     struct in_addr sin_addr;
     * }
     */
    struct sockaddr_in address;

    // 绑定 socket 地址
    memset(&address, 0, sizeof(address)); // 把 address 中的所有数据都设为 0.
    address.sin_family = PF_INET;

    // 绑定到所有地址
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    // 将端口转换为网络字节顺序
    address.sin_port = htons(port);

    // 绑定 socket
    LogMessage(env, obj, "Binding to port %hu.", port);
    /*
     * socket 与地址绑定
     * 新建的 socket 在 socket 族空间中，并没为其分配协议地址。为让客户能定位到这个 socket 并与之相连，需绑定
     * int bind(int socketDescriptor, const struct sockaddr* address, socklen_taddressLength);
     *     > socket描述符：指定将绑定到指定地址的 socket 实例
     *     > address：指定 socket 被绑定的协议地址
     *     > address length：指定传递给函数的协议地址结构的大小
     */
    if (-1 == bind(sd, (struct sockaddr *) &address, sizeof(address))) {
        // 抛出带错误号的异常
        ThrowErrnoException(env, "java/io/IOException", errno);
    }
}

/**
 * 原生辅助函数
 * 获取当前绑定的端口号 socket
 * @param env
 * @param obj
 * @param sd
 * @return
 */
static unsigned short GetSocketPort(JNIEnv *env, jobject obj, int sd) {
    unsigned short port = 0;
    struct sockaddr_in address;
    socklen_t addressLenght = sizeof(address);
    // 获取 socket 地址
    /*
     * __socketcall int getsockname(int __fd, struct sockaddr* __addr, socklen_t* __addr_length);
     */
    if (-1 == getsockname(sd, (struct sockaddr *) &address, &addressLenght)) {
        // 抛出带错误号的异常
        ThrowErrnoException(env, "java/io/IOException", errno);
    } else {
        // 将端口转换为主机字节顺序
        port = ntohs(address.sin_port);
        LogMessage(env, obj, "Binded to random port %hu.", port);
    }
    return port;
}

static void ListenOnSocket(JNIEnv *env, jobject obj, int sd, int backlog) {
    // 监听给定 backlog 的 socket
    LogMessage(env, obj, "Listening on socket with a backlog of %d pending connections.", backlog);

    if (-1 == listen(sd, backlog)) {
        // 抛出带错误号的异常
        ThrowErrnoException(env, "java/io/IOException", errno);
    }
}

/**
 * 记录给定地址的 IP 地址和端口号
 * @param env
 * @param obj
 * @param message 消息文本
 * @param address address实例
 */
static void LogAddress(
        JNIEnv *env, jobject obj, const char *message, const struct sockaddr_in *address) {
    char ip[INET_ADDRSTRLEN];
    // 将 IP 地址转换为字符串
    if (NULL == inet_ntop(PF_INET, &(address->sin_addr), ip, INET_ADDRSTRLEN)) {
        // 抛出带错误号的异常
        ThrowErrnoException(env, "java/io/IOException", errno);
    } else {
        // 将端口号转换为主机字节顺序
        unsigned short port = ntohs(address->sin_port);

        // 记录地址
        LogMessage(env, obj, "%s %s:%hu.", message, ip, port);
    }
}

static int AcceptOnSocket(JNIEnv *env, jobject obj, int sd) {
    struct sockaddr_in address;
    socklen_t addressLength = sizeof(address);

    // 阻塞和等待进来的客户链接
    // 并且接受它
    LogMessage(env, obj, "Waiting for a client connection...");

    int clientSocket = accept(sd, (struct sockaddr *) &address, &addressLength);

    // 如果客户 socket 无效
    if (-1 == clientSocket) {
        // 抛出带错误号的异常
        ThrowErrnoException(env, "java/io/IOException", errno);
    } else {
        // 记录地址
        LogAddress(env, obj, "Client connection from ", &address);
    }

    return clientSocket;
}

/**
 * 阻塞并接收来自 socket 的数据放到缓冲区
 * @param env
 * @param obj
 * @param sd
 * @param buffer
 * @param bufferSize
 * @return
 */
static ssize_t ReceiveFromSocket(
        JNIEnv *env, jobject obj, int sd, char *buffer, size_t bufferSize) {
    // 阻塞并接收来自 socket 的数据放到缓冲区
    LogMessage(env, obj, "Receiving from the socket...");
    /*
     * recv 函数
     *     socket descriptor: 指定接收数据的 socket 实例
     *     buffer pointer: 指向内存地址的指针，用于保存接收数据
     *     buffer length: 数据缓冲区大小
     *     flags: 指定接收所需的额外标志
     */
    ssize_t recvSize = recv(sd, buffer, bufferSize - 1, 0);

    // 如果接收失败
    if (-1 == recvSize) {
        // 抛出带错误号的异常
        ThrowErrnoException(env, "java/io/IOException", errno);
    } else {
        // 以 NULL 结尾缓冲区形成一个字符串
        buffer[recvSize] = NULL;

        // 如果接收数据成功
        if (recvSize > 0) {
            LogMessage(env, obj, "Received %d byte: %s", recvSize, buffer);
        } else {
            LogMessage(env, obj, "Client disconnected");
        }
    }
    return recvSize;
}

/**
 * 将数据缓冲区的数据发送到 socket
 * @param env
 * @param obj
 * @param sd
 * @param buffer
 * @param bufferSize
 * @return
 */
static ssize_t SendToSocket(
        JNIEnv *env, jobject obj, int sd, const char *buffer, size_t bufferSize) {
    // 将数据缓冲区发送到 socket
    LogMessage(env, obj, "Sending to the socket...");
    ssize_t sentSize = send(sd, buffer, bufferSize, 0);

    // 如果发送失败
    if (-1 == sentSize) {
        // 抛出带错误号的异常
        ThrowErrnoException(env, "java/io/IOException", errno);
    } else {
        if (sentSize > 0) {
            LogMessage(env, obj, "Send %d bytes: %s", sentSize, buffer);
        } else {
            LogMessage(env, obj, "Client disconnected.");
        }
    }
    return sentSize;
}

void
Java_com_liu_echo_EchoServerActivity_nativeStartTcpServer(JNIEnv *env, jobject obj, jint port) {
    // 构造新的 TCP socket。
    int serverSocket = NewTcpSocket(env, obj);
    if (NULL == env->ExceptionOccurred()) {
        // 将 socket 绑定到某端口号
        BindSocketToPort(env, obj, serverSocket, (unsigned short) port);
        if (NULL != env->ExceptionOccurred()) {
            goto exit;
        }
        // 如果请求了随机端口号
        if (0 == port) {
            // 获取当前绑定的端口号
            GetSocketPort(env, obj, serverSocket);
            if (NULL != env->ExceptionOccurred()) {
                goto exit;
            }
        }

        // 监听有4个等待连接的 backlog 的 socket
        ListenOnSocket(env, obj, serverSocket, 4);
        if (NULL != env->ExceptionOccurred()) {
            goto exit;
        }

        // 接受 socket 的一个客户连接
        int clientSocket = AcceptOnSocket(env, obj, serverSocket);
        if (NULL != env->ExceptionOccurred()) {
            goto exit;
        }

        char buffer[MAX_BUFFER_SIZE];
        ssize_t recvSize;
        ssize_t sentSize;

        // 接收并发送数据
        while (1) {
            // 从 socket 中接收
            recvSize = ReceiveFromSocket(env, obj, clientSocket, buffer, MAX_BUFFER_SIZE);

            if ((0 == recvSize) || (NULL != env->ExceptionOccurred())) {
                break;
            }

            // 发送给 socket
            sentSize = SendToSocket(env, obj, clientSocket, buffer, (size_t) recvSize);

            if ((0 == sentSize) || (NULL != env->ExceptionOccurred())) {
                break;
            }
        }

        // 关闭客户端
        close(clientSocket);
    }

    exit:
    if (serverSocket > 0) {
        close(serverSocket);
    }
}

static void ConnectToAddress(
        JNIEnv *env, jobject obj, int sd, const char *ip, unsigned short port) {
    // 连接到给定的 IP 地址和端口号
    LogMessage(env, obj, "Connecting to %s:%uh...", ip, port);

    struct sockaddr_in address;

    memset(&address, 0, sizeof(address));
    address.sin_family = PF_INET;

    // 将 IP 地址字符串转换为网络地址
    if (0 == inet_aton(ip, &(address.sin_addr))) {
        // 抛出带错误号的异常
        ThrowErrnoException(env, "java/io/IOException", errno);
    } else {
        // 将端口号转换为网络字节顺序
        address.sin_port = htons(port);
        // 转换为地址
        if (-1 == connect(sd, (const sockaddr *) &address, sizeof(address))) {
            // 抛出带错误号的异常
            ThrowErrnoException(env, "java/io/IOException", errno);
        } else {
            LogMessage(env, obj, "Connected.");
        }
    }
}

void
Java_com_liu_echo_EchoClientActivity_nativeStartTcpClient(JNIEnv *env, jobject obj, jstring ip,
                                                          jint port,
                                                          jstring message) {
    // 构造新的 TCP socket
    int clientSocket = NewTcpSocket(env, obj);
    if (NULL == env->ExceptionOccurred()) {

        // 以 C 字符串形式获取 IP 地址
        const char *ipAddress = env->GetStringUTFChars(ip, NULL);
        if (NULL == ipAddress) {
            goto exit;
        }

        // 连接到 IP 地址和端口
        ConnectToAddress(env, obj, clientSocket, ipAddress, (unsigned short) port);

        // 释放已经用完的 IP 地址，
        env->ReleaseStringUTFChars(ip, ipAddress);

        // 如果连接不成功
        if (NULL != env->ExceptionOccurred()) {
            goto exit;
        }

        // 以 C 字符串的形式获取消息
        const char *messageText = env->GetStringUTFChars(message, NULL);
        if (NULL == messageText) {
            goto exit;
        }

        // 获取消息大小
        jsize messageSize = env->GetStringUTFLength(message);

        // 发送消息给 socket
        SendToSocket(env, obj, clientSocket, messageText, (size_t) messageSize);

        // 释放已经用完的消息文本
        env->ReleaseStringUTFChars(message, messageText);

        // 如果发送未成功
        if (NULL != env->ExceptionOccurred()) {
            goto exit;
        }

        char buffer[MAX_BUFFER_SIZE];

        // 从 socket 接收
        ReceiveFromSocket(env, obj, clientSocket, buffer, MAX_BUFFER_SIZE);
    }
    exit:
    if (clientSocket > -1) {
        close(clientSocket);
    }
}

/**
 * 创建一个新的 UDP socket
 * @param env
 * @param obj
 * @return
 */
static int NewUdpSocket(JNIEnv *env, jobject obj) {
    // 构造 socket
    LogMessage(env, obj, "Constructing a new UDP socket...");
    int udpSocket = socket(PF_INET, SOCK_DGRAM, 0);

    // 检查 socket 构造是否正确
    if (-1 == udpSocket) {
        // 抛出带错误号的异常
        ThrowErrnoException(env, "java/io/IOException", errno);
    }

    return udpSocket;
}

/**
 * 从 socket 中阻塞并接收数据报保存到缓冲区，填充客户端地址
 *
 * @param env
 * @param obj
 * @param sd
 * @param address
 * @param buffer
 * @param bufferSize
 * @return
 */
static ssize_t ReceiveDatagramFromSocket(JNIEnv *env, jobject obj,
                                         int sd,
                                         struct sockaddr_in *address,
                                         char *buffer,
                                         size_t bufferSize) {
    socklen_t addressLength = sizeof(struct sockaddr_in);

    // 从 socket 中接收数据
    LogMessage(env, obj, "Receiving from the socket...");
    /*
     * ssize_t recvfrom(int __fd, void* __buf, size_t __n, int __flags, struct sockaddr* __src_addr, socklen_t* __src_addr_length);
     */
    ssize_t recvSize = recvfrom(sd, buffer, bufferSize, 0, (struct sockaddr *) address,
                                &addressLength);

    if (-1 == recvSize) {
        // 抛出带错误号的异常
        ThrowErrnoException(env, "java/io/IOException", errno);
    } else {
        // 记录地址
        LogAddress(env, obj, "Received from", address);

        // 以 NULL 终止缓冲区使其为一个字符串
        buffer[recvSize] = NULL;

        // 如果数据已经接收
        if (recvSize > 0) {
            LogMessage(env, obj, "Received %d byte: %s", recvSize, buffer);
        }
    }
    return recvSize;
}

/**
 * 用给定的 socket 发送数据报到给定的地址
 * @param env
 * @param obj
 * @param sd
 * @param address
 * @param buffer
 * @param bufferSize
 * @return
 */
static ssize_t SendDatagramToSocket(JNIEnv *env, jobject obj,
                                    int sd,
                                    const struct sockaddr_in *address,
                                    const char *buffer,
                                    size_t bufferSize) {
    // 向 socket 发送数据缓冲区
    LogAddress(env, obj, "Sending to", address);
    ssize_t sentSize = sendto(sd, buffer, bufferSize, 0, (const sockaddr *) address,
                              sizeof(struct sockaddr_in));
    // 如果发送失败
    if (-1 == sentSize) {
        ThrowErrnoException(env, "java/io/IOException", errno);
    } else if (sentSize > 0) {
        LogMessage(env, obj, "Sent %d bytes: %s", sentSize, buffer);
    }
    return sentSize;
}

/**
 * 启动 UDP 服务器
 *     流程：socket->bind->(recvfrom/sendto)->close
 * @param env
 * @param obj
 * @param port
 */
void Java_com_liu_echo_EchoServerActivity_nativeStartUdpServer
        (JNIEnv *env, jobject obj, jint port) {
    // 构造一个新的 UDP socket
    int serverSocket = NewUdpSocket(env, obj);
    if (NULL == env->ExceptionOccurred()) {
        // 将 socket 绑定到某一个端口号
        BindSocketToPort(env, obj, serverSocket, (unsigned short) port);
        if (NULL != env->ExceptionOccurred()) {
            goto exit;
        }

        // 如果请求随机端口号
        if (0 == port) {
            // 获取当前绑定的端口号
            GetSocketPort(env, obj, serverSocket);
            if (NULL != env->ExceptionOccurred()) {
                goto exit;
            }
        }

        // 客户端地址
        struct sockaddr_in address;
        memset(&address, 0, sizeof(address));

        char buffer[MAX_BUFFER_SIZE];
        ssize_t recvSize;
        ssize_t sentSize;

        // 从 socket 中接收
        recvSize = ReceiveDatagramFromSocket(env, obj, serverSocket, &address, buffer,
                                             MAX_BUFFER_SIZE);

        if ((0 == recvSize) || (NULL != env->ExceptionOccurred())) {
            goto exit;
        }

        // 发送给 socket
        sentSize = SendDatagramToSocket(env, obj, serverSocket, &address, buffer,
                                        (size_t) recvSize);
    }

    exit:
    if (serverSocket > 0) {
        close(serverSocket);
    }
}

/**
 * 启动 UDP 客户端
 *     流程：socket->(sendto/recvfrom)->close
 * @param env
 * @param obj
 * @param ip
 * @param port
 * @param message
 */
void Java_com_liu_echo_EchoClientActivity_nativeStartUdpClient
        (JNIEnv *env, jobject obj, jstring ip, jint port, jstring message) {
    // 构造一个新的 UDP socket
    int clientSocket = NewUdpSocket(env, obj);
    if (NULL == env->ExceptionOccurred()) {
        struct sockaddr_in address;

        memset(&address, 0, sizeof(address));
        address.sin_family = PF_INET;

        // 以 C 字符串形式获取 IP 地址
        const char *ipAddress = env->GetStringUTFChars(ip, NULL);
        if (NULL == ipAddress) {
            goto exit;
        }

        // 将 IP 地址字符串转换为网络地址
        /*
         * 完整描述：
         *      int inet_aton(const char *string, struct in_addr *addr);
         * 参数描述：
         *      1 输入参数string包含ASCII表示的IP地址。
         *      2 输出参数addr是将要用新的IP地址更新的结构。
         * 返回值：
         *      如果这个函数成功，函数的返回值非零。如果输入地址不正确则会返回零。
         *  使用这个函数并没有错误码存放在errno中，所以他的值会被忽略。
         */
        int result = inet_aton(ipAddress, &(address.sin_addr));

        // 释放 IP 地址
        env->ReleaseStringUTFChars(ip, ipAddress);

        // 如果转换失败
        if (0 == result) {
            // 抛出带错误号的异常
            ThrowErrnoException(env, "java/io/IOException", errno);
            goto exit;
        }

        // 将端口转换为网络字节顺序
        address.sin_port = htons(port); // host to network short

        // 以 C 字符串形式获取消息
        const char *messageText = env->GetStringUTFChars(message, NULL);
        if (NULL == messageText) {
            goto exit;
        }

        // 获取消息大小
        jsize messageSize = env->GetStringUTFLength(message);

        // 发送消息给 socket
        SendDatagramToSocket(env, obj, clientSocket, &address, messageText, messageSize);

        // 释放消息文本
        env->ReleaseStringUTFChars(message, messageText);

        // 如果消息发送未成功
        if (NULL != env->ExceptionOccurred()) {
            goto exit;
        }

        char buffer[MAX_BUFFER_SIZE];

        // 清除地址
        memset(&address, 0, sizeof(address));

        // 从 socket 接收
        ReceiveDatagramFromSocket(env, obj, clientSocket, &address, buffer, MAX_BUFFER_SIZE);

    }

    exit:
    if (clientSocket > 0) {
        close(clientSocket);
    }
}

/**
 *  构造一个新的原生 UNIX socket
 * @param env JNIEnv 接口
 * @param obj Java 对象实例
 * @return sd socket 描述
 */
static int NewLocalSocket(JNIEnv *env, jobject obj) {
    // 构造 Socket
    LogMessage(env, obj, "Constructing a new local UNIX Socket...");
    int localSocket = socket(PF_LOCAL, SOCK_STREAM, 0);
    // 检查 socket 构造是否正确
    if (-1 == localSocket) {
        // 抛出带错误号的异常
        ThrowErrnoException(env, "java/io/IOException", errno);
    }
    return localSocket;
}

/**
 * 将本地 UNIX socket 与某一名称绑定
 * @param env JNIEnv 接口
 * @param obj Java 对象实例
 * @param sd socket 描述符
 * @param name socket 名称
 */
static void BindLocalSocketToName(JNIEnv *env, jobject obj, int sd, const char *name) {

    /*
    sockaddr_un 地址结构体指定本地 socket 的协议地址
    struct sockaddr_un {
        sa_family_t  sun_family;
        char sun_path[UNIX_PATH_MAX];
    };
     */

    struct sockaddr_un address;

    // 名字长度
    const size_t nameLength = strlen(name);

    // 路径长度初始化与名字长度相等
    size_t pathLength = nameLength;

    // 如果名字不是以'/'开头，即它在抽象命名空间里
    // in the abstract namespace
    bool abstractNamespace = ('/' != name[0]);

    // 抽象命名空间要求目录的第一个字节是 0 字节，更新目录长度包括 0 字节
    if (abstractNamespace) {
        pathLength++;
    }

    // 检查路径长度
    if (pathLength > sizeof(address.sun_path)) {
        // 抛出带错误号的异常
        ThrowException(env, "java/io/IOException", "Name is too big");
    } else {
        // 清除地址字节
        memset(&address, 0, sizeof(address));
        address.sun_family = PF_LOCAL;

        // socket 路径
        char *sunPath = address.sun_path;

        // 第一个字节必须是 0 以使用抽象命名空间
        if (abstractNamespace) {
            *sunPath++ = NULL;
        }

    }
}

void Java_com_liu_echo_LocalSocketActivity_nativeStartLocalServer
        (JNIEnv *, jobject, jstring);