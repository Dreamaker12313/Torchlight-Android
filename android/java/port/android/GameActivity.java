// The game's activity: NativeActivity, which loads libtorchlight.so and runs
// its android_main(), with one change. The phone's Back (on a phone with
// gesture navigation, a swipe in from either side, easy to make while playing
// with the touch pad) sends the game to the background, as Home does, instead
// of finishing the activity and so ending the run. The game is left where it
// was; the engine waits in the background for its window to come back.
package port.android;

import android.app.NativeActivity;
import android.os.Build;
import android.os.Bundle;
import android.window.OnBackInvokedCallback;
import android.window.OnBackInvokedDispatcher;

public final class GameActivity extends NativeActivity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        // From Android 13 Back arrives as a callback (the predictive back
        // gesture); onBackPressed() below is the older phones' path.
        if (Build.VERSION.SDK_INT >= 33) {
            OnBackInvokedCallback toBackground = () -> moveTaskToBack(true);
            getOnBackInvokedDispatcher().registerOnBackInvokedCallback(
                    OnBackInvokedDispatcher.PRIORITY_DEFAULT, toBackground);
        }
    }

    @Override
    @SuppressWarnings("deprecation")
    public void onBackPressed() {
        moveTaskToBack(true);
    }
}
