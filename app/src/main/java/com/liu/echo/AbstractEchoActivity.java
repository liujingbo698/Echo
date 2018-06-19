package com.liu.echo;

import android.app.Activity;
import android.os.Bundle;
import android.os.Handler;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ScrollView;
import android.widget.TextView;

public abstract class AbstractEchoActivity extends Activity implements View.OnClickListener {

    /** 端口号 */
    protected EditText portEdit;

    /** 服务按钮 */
    protected Button startButton;

    /** 日志滚动 */
    protected ScrollView logScroll;

    /** 日志视图 */
    protected TextView logView;

    /** 布局 ID */
    private final int layoutID;

    /**
     * 构造函数
     * @param layoutID
     */
    public AbstractEchoActivity(int layoutID) {
        this.layoutID = layoutID;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(layoutID);

        portEdit = findViewById(R.id.port_edit);
        startButton = findViewById(R.id.start_button);
        logScroll = findViewById(R.id.log_scroll);
        logView = findViewById(R.id.log_view);

        startButton.setOnClickListener(this);

    }

    @Override
    public void onClick(View v) {
        if (v == startButton) {
            onStartButtonClicked();
        }
    }

    /**
     * 在开始按钮上点击
     */
    protected abstract void onStartButtonClicked();

    /**
     * 以整型获取接口号
     * @return
     */
    protected Integer getPort(){
        Integer port;

        try {
            port = Integer.valueOf(portEdit.getText().toString());
        } catch (NumberFormatException e) {
            port = null;
        }

        return port;
    }

    /**
     * 记录给定的消息
     * @param message
     */
    protected void logMessage(final String message) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                logMessageDirect(message);
            }
        });
    }

    /**
     * 直接记录给定的消息
     * @param message
     */
    protected void logMessageDirect(String message) {
        logView.append(message);
        logView.append("\n");
        logScroll.fullScroll(View.FOCUS_DOWN);
    }

    /**
     * 抽象异步 echo 任务
     */
    protected abstract class AbstractEchoTask extends Thread{
        /** Handler 对象。 */
        private final Handler handler;

        /**
         * 构造函数
         */
        public AbstractEchoTask() {
            this.handler = new Handler();
        }

        /**
         * 在调用线程中先执行回调
         */
        protected void onPreExecute(){
            startButton.setEnabled(false);
            logView.setText("");
        }

        public synchronized void start(){
            onPreExecute();
            super.start();
        }

        @Override
        public void run() {
            onBackground();
            handler.post(new Runnable() {
                @Override
                public void run() {
                    onPostExecute();
                }
            });
        }

        /**
         * 新线程中的背景回调
         */
        protected abstract void onBackground();

        /**
         * 在调用线程中后执行回调
         */
        protected void onPostExecute(){
            startButton.setEnabled(true);
        }
    }

    static {
        System.loadLibrary("Echo");
    }
}
