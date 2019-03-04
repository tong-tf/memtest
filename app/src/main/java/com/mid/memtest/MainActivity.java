package com.mid.memtest;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Message;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;

import java.nio.Buffer;

import butterknife.BindString;
import butterknife.BindView;
import butterknife.ButterKnife;
import butterknife.OnClick;

public class MainActivity extends AppCompatActivity {
    private final static String TAG = "tong";
    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("native-lib");
    }

    private final static int MSG_TEST_DONE = 0;
    private final static int MSG_TEST_START = 1;
    private final static int MSG_TEST_UPDATE = 2;

    @BindView(R.id.mem_count)
    EditText memCount;
    @BindView(R.id.mem_size)
    EditText memSize;
    @BindView(R.id.rv_result)
    TextView testResult;
    @BindView(R.id.test_mem)
    Button testMem;


    private HandlerThread mMemTestThread;
    private MemTestHandler memTestHandler;

    SharedPreferences mSp;
    @Override
    protected void onCreate(Bundle savedInstanceState) {

        super.onCreate(savedInstanceState);
        setUp();
        setContentView(R.layout.activity_main);
        ButterKnife.bind(this);
        startMemTest();
        mSp = getPreferences(Context.MODE_PRIVATE);
        memCount.setText(mSp.getString("count", "10"));
        memSize.setText(mSp.getString("size", "64M"));

    }


    private void startMemTest(){
        mMemTestThread = new HandlerThread("Memory Test");
        mMemTestThread.start();
        memTestHandler = new MemTestHandler(mMemTestThread.getLooper(), h);

    }

    @OnClick({R.id.test_mem})
    public void MyClick(View view){
        switch (view.getId()){
            case R.id.test_mem:
                //TODO: begin test
                String size = memSize.getText().toString();
                String count = memCount.getText().toString();
                Bundle data = new Bundle();
                data.putString("size", size);
                data.putString("count", count);
                SharedPreferences.Editor editor = mSp.edit();
                editor.putString("size", size);
                editor.putString("count",count);
                editor.commit();
                Log.i(TAG, String.format("Test condition: %s, %s", size, count));
                memTestHandler.sendMessage(memTestHandler.obtainMessage(MSG_TEST_START, data));
                view.setEnabled(false);
                testResult.setText(getResources().getString(R.string.test_result));
                break;
        }
    }


    private final class MemTestHandler extends  Handler {
        private Handler mReply;
        public MemTestHandler(Looper looper, Handler reply){
            super(looper); mReply = reply;
        }

        @Override
        public void handleMessage(Message msg) {
            switch (msg.what){
                case MSG_TEST_START:
                    Bundle data = (Bundle) msg.obj;
                    int rv = run_memtest(data.getString("size"), data.getString("count"));
                    mReply.sendMessage(mReply.obtainMessage(MSG_TEST_DONE, rv , 0));
                    break;
            }
        }
    }


    private final Handler h = new Handler(){
        @Override
        public void handleMessage(Message msg) {
            switch (msg.what){
                case MSG_TEST_DONE:
                    String text = String.format(getResources().getString(R.string.test_result), MemtestParser.parse(msg.arg1));
                    testResult.setText(text);
                    testMem.setEnabled(true);
                    break;
                case MSG_TEST_UPDATE:
                    String obj = (String) msg.obj;
                    if(obj.startsWith("Loop")){  // a new test begin, clear previous result.
                        testResult.setText(String.format(getResources().getString(R.string.test_result), obj));
                    }else{
                        testResult.setText(testResult.getText().toString() + "\n" + obj);
                    }
                    break;
            }
        }
    };


    public void updateProgress(String msg){
        h.sendMessage(h.obtainMessage(MSG_TEST_UPDATE, msg));
    }
    /**
     * A native method that is implemented by the 'native-lib' native library,
     * which is packaged with this application.
     */
    public native  String stringFromJNI();
    public native int run_memtest(String size, String count);
    public native void setUp();
}
