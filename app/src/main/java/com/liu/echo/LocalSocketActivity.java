package com.liu.echo;

import android.net.LocalSocket;
import android.net.LocalSocketAddress;
import android.os.Bundle;
import android.widget.EditText;

import java.io.File;
import java.io.InputStream;
import java.io.OutputStream;

public class LocalSocketActivity extends AbstractEchoActivity {
    /**
     * 消息编辑
     */
    private EditText messageEdit;

    /**
     * 构造函数
     */
    public LocalSocketActivity() {
        super(R.layout.activity_echo_local);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        messageEdit = findViewById(R.id.message_edit);
    }

    @Override
    protected void onStartButtonClicked() {
        String name = portEdit.getText().toString();
        String message = messageEdit.getText().toString();

        if ((name.length() > 0) && (message.length() > 0)) {
            String socketName;

            // 如果是 filesystem socket 预先准备应用程序的文件目录
            if (isFilesystemSocket(name)) {
                File file = new File(getFilesDir(), name);
                socketName = file.getAbsolutePath();
            } else {
                socketName = name;
            }

            ServerTask serverTask = new ServerTask(socketName);
            serverTask.start();

            ClientTask clientTask = new ClientTask(socketName, message);
            clientTask.start();

        }

    }

    /**
     * 检查名称是否是 filesystem socket
     *
     * @param name 名称
     * @return 是否文件开头
     */
    private boolean isFilesystemSocket(String name) {
        return name.startsWith("/");
    }

    /**
     * 启动绑定到给定名称的本地 UNIX socket 服务器
     *
     * @param name 名称
     * @throws Exception 可能的IO流异常
     */
    private native void nativeStartLocalServer(String name) throws Exception;

    /**
     * 启动本地 UNIX socket 客户端
     * @param name 名称
     * @param message 消息
     * @throws Exception 可能的异常
     */
    private void startLocalClient(String name, String message) throws Exception {
        // 构造一个本地 socket
        LocalSocket clientSocket = new LocalSocket();
        try {
            // 设置 socket 名称空间
            LocalSocketAddress.Namespace namespace;
            if (isFilesystemSocket(name)) {
                namespace = LocalSocketAddress.Namespace.FILESYSTEM;
            } else {
                namespace = LocalSocketAddress.Namespace.ABSTRACT;
            }
            // 构造本地 socket 地址
            LocalSocketAddress address = new LocalSocketAddress(name, namespace);

            // 连接到本地 socket
            logMessage("Connecting to " + name);
            clientSocket.connect(address);
            logMessage("Connected.");
            // 以字节形式获取消息
            byte[] messageBytes = message.getBytes();
            // 发送消息字节到 socket
            logMessage("Sending to the socket...");
            OutputStream outputStream = clientSocket.getOutputStream();
            outputStream.write(messageBytes);
            logMessage(String.format("Sent %d bytes: %s", messageBytes.length, message));

            // 从 socket 中接收消息返回
            logMessage("Receiving from the socket...");
            InputStream inputStream = clientSocket.getInputStream();
            int readSize = inputStream.read(messageBytes);

            String receivedMessage = new String(messageBytes, 0, readSize);
            logMessage(String.format("Received %d bytes: %s", readSize, receivedMessage));

            // 关闭流
            inputStream.close();
            outputStream.close();
        } finally {
            // 关闭本地 socket
            clientSocket.close();
        }
    }

    /**
     * 服务器任务
     */
    private class ServerTask extends AbstractEchoTask {
        /**
         * Socket 名称
         */
        private final String name;

        /**
         * 构造函数
         *
         * @param name socket 名称
         */
        public ServerTask(String name) {
            this.name = name;
        }

        @Override
        protected void onBackground() {
            logMessage("Starting server.");
            try {
                nativeStartLocalServer(name);
            } catch (Exception e) {
                logMessage(e.getMessage());
            }
            logMessage("Server terminated.");
        }
    }

    private class ClientTask extends Thread {
        /**
         * Socket 名称
         */
        private final String name;

        /**
         * 发送的文本消息
         */
        private final String message;

        public ClientTask(String name, String message) {
            this.name = name;
            this.message = message;
        }

        @Override
        public void run() {
            logMessage("Starting client.");

            try {
                startLocalClient(name, message);
            } catch (Exception e) {
                logMessage(e.getMessage());
            }

            logMessage("Client terminated.");
        }
    }
}
