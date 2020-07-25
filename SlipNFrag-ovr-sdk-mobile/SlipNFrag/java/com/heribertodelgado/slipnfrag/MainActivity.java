package com.heribertodelgado.slipnfrag;

import android.Manifest;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.util.Log;

public class MainActivity extends android.app.NativeActivity implements ActivityCompat.OnRequestPermissionsResultCallback
{
  static 
  {
    System.loadLibrary("vrapi");
    System.loadLibrary("slipnfrag");
  }

  public static final String TAG = "SlipNFrag";

  public static native void notifyPermissionsGrantStatus(int permissionsGranted);

  boolean externalStoragePermissionGranted = false;

  @Override
  protected void onCreate(Bundle savedInstanceState)
  {
    Log.d(TAG, "onCreate");
    super.onCreate(savedInstanceState);
    if (ContextCompat.checkSelfPermission(this, Manifest.permission.READ_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED)
    {
      Log.d(TAG, "onCreate - requesting necessary permissions.");
      ActivityCompat.requestPermissions(this, new String[] { Manifest.permission.READ_EXTERNAL_STORAGE }, 0);
    }
    else
    {
      Log.d(TAG, "onCreate - permissions already granted.");
      externalStoragePermissionGranted = true;
      notifyPermissionsGrantStatus(1);
    }
  }

  @Override
  public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults)
  {
    for (int i = 0; i < permissions.length; i++)
    {
      switch (permissions[i])
      {
        case Manifest.permission.READ_EXTERNAL_STORAGE:
          if (grantResults[i] == PackageManager.PERMISSION_GRANTED)
          {
            Log.d(TAG, "onRequestPermissionsResult: permission GRANTED");
            externalStoragePermissionGranted = true;
            runOnUiThread(new Thread()
            {
              @Override
              public void run()
              {
                notifyPermissionsGrantStatus(1);
              }
            });
          }
          else
          {
            Log.d(TAG, "onRequestPermissionsResult: permission DENIED");
            externalStoragePermissionGranted = false;
            notifyPermissionsGrantStatus(0);
          }
          return;
      }
    }
  }
}
