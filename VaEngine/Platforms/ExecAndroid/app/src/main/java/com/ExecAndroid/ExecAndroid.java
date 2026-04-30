package com.ExecAndroid;

import android.app.Activity;
import android.widget.TextView;
import android.os.Bundle;

public class ExecAndroid extends Activity
{
    /** 작업을 처음 만들 때 호출됩니다. */
    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);

        /* TextView를 만들고 텍스트를 "Hello world"로 설정 */
        TextView  tv = new TextView(this);
        tv.setText("Hello World!");
        setContentView(tv);
    }
}
