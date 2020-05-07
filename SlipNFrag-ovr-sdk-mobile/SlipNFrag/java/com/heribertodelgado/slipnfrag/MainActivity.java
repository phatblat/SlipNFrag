package com.heribertodelgado.slipnfrag;

public class MainActivity extends android.app.NativeActivity 
{
  static 
  {
    System.loadLibrary("vrapi");
    System.loadLibrary("slipnfrag");
  }
}
