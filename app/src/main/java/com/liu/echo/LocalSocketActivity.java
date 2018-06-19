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

        }

    }

    /**
     * 检查名称是否是 filesystem socket
     *
     * @param name
     * @return
     */
    private boolean isFilesystemSocket(String name) {
        return name.startsWith("/");
    }

    /**
     * 启动绑定到给定名称的本地 UNIX socket 服务器
     *
     * @param name
     * @throws Exception
     */
    private native void nativeStartLocalServer(String name) throws Exception;

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
            logMessage(String.format("Sent %d bytes: %s", messageBytes.length, message);

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
}
