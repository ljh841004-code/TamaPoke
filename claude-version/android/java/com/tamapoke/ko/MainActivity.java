package com.tamapoke.ko;

import android.app.NativeActivity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.Uri;
import android.os.BatteryManager;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.view.View;
import android.view.WindowManager;
import android.widget.Toast;

// El juego entero es nativo (libtamapoke.so, el mismo codigo del firmware). Aqui solo:
// elegir mons.pak con el selector de archivos del sistema, la bateria, y pantalla completa.
public class MainActivity extends NativeActivity {
  static { System.loadLibrary("tamapoke"); }
  static native void nativePakFds(int[] fds);
  static native void nativeBattery(int pct, boolean charging);

  private static final int PICK_PAK = 1;

  static class BatteryRx extends BroadcastReceiver {
    @Override public void onReceive(Context c, Intent i) {
      int level = i.getIntExtra(BatteryManager.EXTRA_LEVEL, -1);
      int scale = i.getIntExtra(BatteryManager.EXTRA_SCALE, 100);
      int st = i.getIntExtra(BatteryManager.EXTRA_STATUS, -1);
      boolean chg = st == BatteryManager.BATTERY_STATUS_CHARGING || st == BatteryManager.BATTERY_STATUS_FULL;
      if (level >= 0 && scale > 0) nativeBattery(level * 100 / scale, chg);
    }
  }
  private final BatteryRx battery = new BatteryRx();

  // (sin clases anonimas: el d8 de build-tools 34 falla con ellas en este JDK)
  static class PickRunner implements Runnable {
    final MainActivity a;
    PickRunner(MainActivity a) { this.a = a; }
    @Override public void run() { a.openPicker(); }
  }

  @Override protected void onCreate(Bundle b) {
    super.onCreate(b);
    getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    registerReceiver(battery, new IntentFilter(Intent.ACTION_BATTERY_CHANGED));
  }

  @Override protected void onDestroy() {
    try { unregisterReceiver(battery); } catch (Exception e) { }
    super.onDestroy();
  }

  @Override public void onWindowFocusChanged(boolean has) {
    super.onWindowFocusChanged(has);
    if (has) {
      getWindow().getDecorView().setSystemUiVisibility(
          View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_FULLSCREEN
          | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
          | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);
    }
  }

  // la llama el codigo nativo (primera vez sin mons, o el menu de la tecla atras)
  public void pickPak() {
    runOnUiThread(new PickRunner(this));
  }

  void openPicker() {
    {
      Intent it = new Intent(Intent.ACTION_OPEN_DOCUMENT);
      it.addCategory(Intent.CATEGORY_OPENABLE);
      it.setType("*/*");
      it.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);  // mons.pak entero o todos sus trozos juntos
      try {
        startActivityForResult(it, PICK_PAK);
        Toast.makeText(this, "mons.pak 을 골라 주세요 (조각이면 모두 길게 눌러 함께 선택)", Toast.LENGTH_LONG).show();
      } catch (Exception e) {
        Toast.makeText(this, "파일 고르기를 열 수 없어요", Toast.LENGTH_LONG).show();
      }
    }
  }

  @Override protected void onActivityResult(int req, int res, Intent data) {
    super.onActivityResult(req, res, data);
    if (req != PICK_PAK || res != RESULT_OK || data == null) return;
    java.util.ArrayList<Uri> uris = new java.util.ArrayList<Uri>();
    if (data.getClipData() != null) {
      for (int i = 0; i < data.getClipData().getItemCount(); i++) uris.add(data.getClipData().getItemAt(i).getUri());
    } else if (data.getData() != null) {
      uris.add(data.getData());
    }
    // trozos por nombre: mons.pak.001, mons.pak.002 ...
    final java.util.HashMap<Uri, String> names = new java.util.HashMap<Uri, String>();
    for (Uri u : uris) names.put(u, nameOf(u));
    java.util.Collections.sort(uris, new ByName(names));
    int[] fds = new int[uris.size()];
    try {
      for (int i = 0; i < uris.size(); i++) {
        ParcelFileDescriptor pfd = getContentResolver().openFileDescriptor(uris.get(i), "r");
        fds[i] = pfd.detachFd();
      }
      nativePakFds(fds);
    } catch (Exception e) {
      Toast.makeText(this, "파일을 열 수 없어요", Toast.LENGTH_LONG).show();
    }
  }

  static class ByName implements java.util.Comparator<Uri> {
    final java.util.HashMap<Uri, String> n;
    ByName(java.util.HashMap<Uri, String> n) { this.n = n; }
    @Override public int compare(Uri a, Uri b) { return n.get(a).compareTo(n.get(b)); }
  }

  String nameOf(Uri u) {
    String name = u.getLastPathSegment();
    android.database.Cursor c = null;
    try {
      c = getContentResolver().query(u, new String[] { android.provider.OpenableColumns.DISPLAY_NAME }, null, null, null);
      if (c != null && c.moveToFirst()) name = c.getString(0);
    } catch (Exception e) {
    } finally {
      if (c != null) c.close();
    }
    return name == null ? "" : name;
  }
}
