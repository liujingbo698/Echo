package com.liu.echo;

import android.os.Bundle;
import android.widget.EditText;

/**
 * Echo 客户端
 */
public class EchoClientActivity extends AbstractEchoActivity {

    /**
     * IP 地址
     */
    private EditText ipEdit;

    /**
     * 消息编辑
     */
    private EditText messageEdit;

    /**
     * 构造函数
     */
    public EchoClientActivity() {
        super(R.layout.activity_echo_client);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        ipEdit = findViewById(R.id.ip_edit);
        messageEdit = findViewById(R.id.message_edit);
    }

    @Override
    protected void onStartButtonClicked() {
        String ip = ipEdit.getText().toString();
        Integer port = getPort();
        String message = messageEdit.getText().toString();

        if ((0 != ip.length()) && (port != null) && (0 != message.length())) {
            ClientTask clientTask = new ClientTask(ip, port, message);
            clientTask.start();
        }
    }

    /**
     * 根据给定服务器 IP 地址和端口号启动 TCP 客户端，并发送给定消息
     *
     * @param ip
     * @param port
     * @param message
     * @throws Exception
     */
    private native void nativeStartTcpClient(String ip, int port, String message) throws Exception;

    private native void nativeStartUdpClient(String ip, int port, String message) throws Exception;

    private class ClientTask extends AbstractEchoTask {
        /**
         * 连接的 IP 地址
         */
        private final String ip;

        /**
         * 端口号
         */
        private final int port;

        /**
         * 发送的消息文本
         */
        private final String message;

        /**
         * 构造函数
         *
         * @param ip
         * @param port
         * @param message
         */
        public ClientTask(String ip, int port, String message) {
            this.ip = ip;
            this.port = port;
            this.message = message;
        }

        @Override
        protected void onBackground() {
            logMessage("Starting client.");
            try {
                nativeStartUdpClient(ip, port, message);
            } catch (Throwable e) {
                logMessage(e.getMessage());
            }

            logMessage("Client terminated.");
        }
    }
}
