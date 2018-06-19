package com.liu.echo;

public class EchoServerActivity extends AbstractEchoActivity {

    /**
     * 构造函数
     */
    public EchoServerActivity() {
        super(R.layout.activity_echo_server);
    }

    @Override
    protected void onStartButtonClicked() {
        Integer port = getPort();
        if (port != null) {
            ServerTask serverTask = new ServerTask(port);
            serverTask.start();
        }
    }

    /**
     * 根据给定端口启动TCP服务器
     * @param port
     * @throws Exception
     */
    private native void nativeStartTcpServer(int port) throws Exception;

    /**
     * 根据给定端口启动UDP服务
     * @param port
     * @throws Exception
     */
    private native void nativeStartUdpServer(int port) throws Exception;

    /**
     * 服务器端任务
     */
    private class ServerTask extends AbstractEchoTask{
        /** 端口号 */
        private final int port;

        /**
         * 构造函数
         * @param port
         */
        public ServerTask(int port) {
            this.port = port;
        }

        @Override
        protected void onBackground() {
            logMessage("Starting server.");
            try {
                nativeStartUdpServer(port);
            } catch (Exception e) {
                logMessage(e.getMessage());
            }
            logMessage("Server terminated.");
        }
    }
}
