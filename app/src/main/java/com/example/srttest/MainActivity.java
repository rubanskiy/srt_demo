package com.example.srttest;

import android.app.Activity;
import android.os.Bundle;
import android.widget.TextView;

public class MainActivity extends Activity {

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    setContentView(R.layout.activity_main);
    TextView tv = findViewById(R.id.sample_text);
    tv.setText(stringFromJNI());
  }

  public native String stringFromJNI();

  static {
    System.loadLibrary("native-lib");
  }
}
