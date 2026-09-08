package me.rocks.grainstorm;
import android.app.Activity;
import android.content.pm.PackageManager;
import android.os.Handler;
import android.os.Looper;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import android.util.Log;
import androidx.core.app.ActivityCompat;
public class PermissionHandler {

    // Add these to your class
    private static CountDownLatch permissionLatch = null;
    private static final AtomicBoolean permissionGranted = new AtomicBoolean(false);
    private static final int PERMISSION_REQUEST_CODE = 1001;

    /**
     * Request permissions and block until result is received
     * @param activity The activity to request permissions from
     * @param permissions Array of permissions to request
     * @param timeoutSeconds Maximum time to wait for user response (e.g., 60 seconds)
     * @return true if all permissions granted, false otherwise
     */
    static boolean requestPermissionsBlocking(Activity activity, String[] permissions, int timeoutSeconds) {
        if (activity == null || permissions == null || permissions.length == 0) {
            return false;
        }

        // Check if permissions are already granted
        boolean allGranted = true;
        for (String permission : permissions) {
            if (ActivityCompat.checkSelfPermission(activity, permission) != PackageManager.PERMISSION_GRANTED) {
                allGranted = false;
                break;
            }
        }

        if (allGranted) {
            Log.d("Permissions", "All permissions already granted");
            return true;
        }

        // Create a latch to wait for the result
        permissionLatch = new CountDownLatch(1);
        permissionGranted.set(false);

        // Request permissions on UI thread
        new Handler(Looper.getMainLooper()).post(new Runnable() {
            @Override
            public void run() {
                activity.requestPermissions(permissions, PERMISSION_REQUEST_CODE);
            }
        });

        try {
            // Block and wait for result (with timeout)
            boolean completed = permissionLatch.await(timeoutSeconds, TimeUnit.SECONDS);

            if (!completed) {
                Log.e("Permissions", "Timeout waiting for permission result");
                return false;
            }

            return permissionGranted.get();

        } catch (InterruptedException e) {
            Log.e("Permissions", "Interrupted while waiting for permissions", e);
            Thread.currentThread().interrupt();
            return false;
        } finally {
            permissionLatch = null;
        }
    }

    /**
     * Call this from your Activity's onRequestPermissionsResult
     */
    static void onPermissionResult(int requestCode, String[] permissions, int[] grantResults) {
        if (requestCode == PERMISSION_REQUEST_CODE && permissionLatch != null) {
            boolean allGranted = true;

            if (grantResults.length > 0) {
                for (int result : grantResults) {
                    if (result != PackageManager.PERMISSION_GRANTED) {
                        allGranted = false;
                        break;
                    }
                }
            } else {
                allGranted = false;
            }

            permissionGranted.set(allGranted);
            permissionLatch.countDown(); // Unblock the waiting thread

            Log.d("Permissions", "Permission result: " + (allGranted ? "granted" : "denied"));
        }
    }


}
