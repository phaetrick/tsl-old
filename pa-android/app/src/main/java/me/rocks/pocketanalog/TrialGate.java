package me.rocks.pocketanalog;

import android.annotation.SuppressLint;
import android.content.Context;
import android.util.Log;
import android.provider.Settings;

import org.bouncycastle.crypto.params.Ed25519PublicKeyParameters;
import org.bouncycastle.crypto.signers.Ed25519Signer;
import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.concurrent.atomic.AtomicBoolean;

import javax.net.ssl.HttpsURLConnection;

/**
 * The trial gate.
 *
 * A device gets 14 days of the full instrument. Enforcement needs the network
 * exactly once, to mint a signed token; from then on the token is local and the
 * device can prove its own state offline -- including proving the trial is over.
 *
 * What the network is NOT allowed to do is decide whether the app runs. An
 * unreachable server is indistinguishable from a captive portal, a plane, or a
 * phone with data switched off, so it resolves to STATE_NONE and the caller
 * falls back to capped sessions. Never to a grant, and never to a block.
 *
 * Nothing here is a security boundary against someone who can patch the APK.
 * It defends against the three things that cost nothing to try: winding the
 * clock back, copying somebody else's token, and clearing app data.
 */
final class TrialGate {

    /** No usable token. Caller runs capped sessions and we retry the mint. */
    static final int STATE_NONE = 0;
    /** Verified, device-bound, unexpired. Full instrument, no limit. */
    static final int STATE_TRIAL = 1;
    /** Verified and past its end. Blocked. */
    static final int STATE_EXPIRED = 2;

    private static final String ENDPOINT =
            "https://thesecretlaboratory.com/api/license/device-trial";

    /**
     * Raw 32-byte Ed25519 public key, base64. The private half never leaves the
     * server; this one is safe to ship, which is the whole reason the tokens are
     * Ed25519 rather than an HMAC (an HMAC key in the binary would let anyone
     * who extracted it mint their own trial forever).
     */
    private static final String PUBLIC_KEY_B64 =
            "Tmy5sN7Z1aq9gqucm1msnOFNyTKgPA3NPvp96r2rEss=";

    /** Payload kind. A licence token is signed by the same key; only this tells them apart. */
    private static final String KIND = "dt";

    /**
     * Mixed into the device hash so the stored value is not a bare sha256 of an
     * ANDROID_ID. Marginal, but free -- and it means the server's table cannot be
     * matched against a list of known device ids computed elsewhere.
     */
    private static final String SALT = "vLt/trial/1";

    private static final String PREF_TOKEN = "tg_t";
    private static final String PREF_HIGHWATER = "tg_hw";
    /** Set once the server has ACKed this device's expiry report. */
    private static final String PREF_EXPIRED_REPORTED = "tg_exp_rep";

    private static final AtomicBoolean running = new AtomicBoolean(false);

    /**
     * Set once Play has authoritatively said this device does NOT own the
     * product. Only then may the gate mint, because minting writes a permanent
     * row and starts a 14-day clock -- doing that before the purchase answer
     * lands burns a trial a paying customer never asked for and never sees.
     */
    private static final AtomicBoolean armed = new AtomicBoolean(false);

    private TrialGate() {}

    // ---------------------------------------------------------------- device

    /**
     * sha256(ANDROID_ID + salt).
     *
     * On API 26+ ANDROID_ID is scoped per app-signing-key, per user, per device,
     * so it SURVIVES uninstall and reinstall and only resets on a factory reset.
     * That property is the entire reason clearing app data does not hand out a
     * fresh trial. The raw value never leaves the phone.
     */
    @SuppressLint("HardwareIds")
    static String deviceHash(Context c) {
        try {
            String id = Settings.Secure.getString(c.getContentResolver(),
                    Settings.Secure.ANDROID_ID);
            if (id == null) id = "";
            MessageDigest md = MessageDigest.getInstance("SHA-256");
            byte[] d = md.digest((id + SALT).getBytes(StandardCharsets.UTF_8));
            StringBuilder sb = new StringBuilder(64);
            for (byte b : d) sb.append(Character.forDigit((b >> 4) & 0xF, 16))
                               .append(Character.forDigit(b & 0xF, 16));
            return sb.toString();
        } catch (Exception e) {
            return null;
        }
    }

    // ----------------------------------------------------------------- entry

    /**
     * Resolve the state and push it to native. Safe to call on any thread; the
     * work happens on its own. Only one pass runs at a time.
     *
     * Call this only when Play has NOT reported a purchase -- a purchased device
     * has nothing to resolve and must never be gated on our server.
     */
    /**
     * Evaluate the stored token and push the state. Never touches the network, so
     * it is safe before Play has answered: it can only report a trial this device
     * already has, never begin one.
     */
    static void refresh(Context c) {
        if (c == null) return;
        push(cachedState(c));
    }

    /**
     * Play has confirmed this device does not own the product. From here the gate
     * may mint. Call once the answer is authoritative -- never speculatively.
     */
    static void arm(Context c) {
        armed.set(true);
        start(c);
    }

    static void start(final Context c) {
        if (c == null) return;
        if (!armed.get()) return;          // no purchase answer yet: do not mint
        if (!running.compareAndSet(false, true)) return;
        final Context app = c.getApplicationContext();
        new Thread(new Runnable() {
            @Override public void run() {
                int state = STATE_NONE;
                try {
                    // Push what we already know before touching the network.
                    // setup_main_window can reach startSessionCap long before a
                    // round trip finishes, and a device whose trial is already
                    // over should not get a free ten minutes while we ask.
                    int known = cachedState(app);
                    if (known != STATE_NONE) push(known);
                    state = resolve(app);
                } catch (Throwable t) {
                    // Any failure at all resolves to NONE -- capped sessions,
                    // retried next launch. Never a grant, never a block.
                    state = STATE_NONE;
                } finally {
                    running.set(false);
                }
                push(state);
                // Tell the server the wall is up, once per install. Only from
                // here -- this thread runs strictly after Play said unpurchased
                // (armed), so a paying customer with a stale expired token never
                // reports. Fire-and-forget: a failure costs nothing and the next
                // launch retries, because the pref is only set on an ack.
                if (state == STATE_EXPIRED) reportExpiredOnce(app);
            }
        }, "trial-gate").start();
    }

    /**
     * One POST {dev, product, event:"expired"} so the backend's funnel sees the
     * trial run out, not just start. Same endpoint, same shape rules. The answer
     * is only an ack; nothing here changes the device's state.
     */
    private static void reportExpiredOnce(Context c) {
        try {
            if (Activities.getDefaultsLong(PREF_EXPIRED_REPORTED, 0L, c) != 0L) return;
            String dev = deviceHash(c);
            if (dev == null) return;

            HttpURLConnection conn = null;
            try {
                conn = (HttpsURLConnection) new URL(ENDPOINT).openConnection();
                conn.setRequestMethod("POST");
                conn.setConnectTimeout(5000);
                conn.setReadTimeout(7000);
                conn.setDoOutput(true);
                conn.setRequestProperty("Content-Type", "application/json");
                conn.setRequestProperty("Accept", "application/json");

                JSONObject body = new JSONObject();
                body.put("dev", dev);
                body.put("product", MainActivity.PRODUCT_ID);
                body.put("event", "expired");
                byte[] out = body.toString().getBytes(StandardCharsets.UTF_8);

                OutputStream os = conn.getOutputStream();
                os.write(out);
                os.close();

                if (conn.getResponseCode() == 200)
                    Activities.setDefaultsLong(PREF_EXPIRED_REPORTED,
                            System.currentTimeMillis(), c);
            } finally {
                if (conn != null) conn.disconnect();
            }
        } catch (Throwable t) {
            // Exception class only -- same rule as mint: never the hash or body.
            Log.w("TrialGate", "expiry report failed: " + t.getClass().getSimpleName());
        }
    }

    private static void push(int state) {
        try {
            MyApplication.java_set_trial_state(state);
        } catch (Throwable ignored) {
            // Native library not loaded yet. Harmless: the gate is re-run on the
            // next launch, and nothing here grants anything on its own.
        }
    }

    /**
     * One line for the Upgrade row in Settings, or null when there is nothing to
     * say (a purchased device has no trial and must not be told about one).
     *
     * Reads the stored token only -- no network, no side effects beyond the
     * high-water write evaluate() already does. Without this the trial is
     * invisible: it runs silently for fourteen days and then simply stops, which
     * is both a bad surprise and impossible to test without waiting out a
     * ten-minute session to see whether a wall arrives.
     */
    static String statusSummary(Context c) {
        if (c == null) return null;
        try {
            String token = Activities.getDefaultsString(PREF_TOKEN, "", c);
            switch (evaluate(c, token)) {
                case STATE_TRIAL: {
                    long left = endsAt(c) - System.currentTimeMillis() / 1000L;
                    long days = (left + 86399L) / 86400L;     // round up: the last
                                                              // part-day is still a day
                    // Status, not a pitch. The row says where this device stands;
                    // the selling happens on the page behind it.
                    return days <= 1 ? "Trial - last day"
                                     : "Trial - " + days + " days left";
                }
                case STATE_EXPIRED:
                    return "Trial ended";
                default:
                    return "Trial not started - no connection";
            }
        } catch (Throwable t) {
            return null;
        }
    }

    /** The `ends` stamp of the stored token, or 0 if there isn't a usable one. */
    private static long endsAt(Context c) {
        try {
            String token = Activities.getDefaultsString(PREF_TOKEN, "", c);
            int dot = token.indexOf('.');
            if (dot <= 0) return 0L;
            byte[] payload = b64(token.substring(0, dot));
            if (payload == null) return 0L;
            return new JSONObject(new String(payload, StandardCharsets.UTF_8))
                    .optLong("ends", 0L);
        } catch (Throwable t) {
            return 0L;
        }
    }

    /** The last state resolved in this install, without any network. */
    static int cachedState(Context c) {
        try {
            return evaluate(c, Activities.getDefaultsString(PREF_TOKEN, "", c));
        } catch (Throwable t) {
            return STATE_NONE;
        }
    }

    // ----------------------------------------------------------------- gates

    private static int resolve(Context c) {
        String token = Activities.getDefaultsString(PREF_TOKEN, "", c);
        int state = evaluate(c, token);
        if (state != STATE_NONE) return state;

        // No usable token: ask the server. A verified answer is stored and the
        // gates run again over it -- including a signed "expired", which is how a
        // device that wiped its token lands back on blocked instead of renewed.
        String minted = mint(c);
        if (minted == null) return STATE_NONE;

        int fresh = evaluate(c, minted);
        if (fresh == STATE_NONE) return STATE_NONE;   // signed for someone else
        Activities.setDefaultsString(PREF_TOKEN, minted, c);
        return fresh;
    }

    /**
     * Gates 02-04: signature, then binding, then clock, then expiry. Anything
     * that fails is treated as no token at all, never as an answer.
     */
    private static int evaluate(Context c, String token) {
        if (token == null || token.isEmpty()) return STATE_NONE;

        final int dot = token.indexOf('.');
        if (dot <= 0 || dot == token.length() - 1) return STATE_NONE;
        final String payloadB64 = token.substring(0, dot);

        byte[] sig = b64(token.substring(dot + 1));
        byte[] payload = b64(payloadB64);
        if (sig == null || payload == null || sig.length != 64) return STATE_NONE;

        // Gate 02 -- Ed25519 over the base64url payload text exactly as sent.
        if (!verify(payloadB64.getBytes(StandardCharsets.US_ASCII), sig)) return STATE_NONE;

        final JSONObject o;
        try {
            o = new JSONObject(new String(payload, StandardCharsets.UTF_8));
        } catch (Exception e) {
            return STATE_NONE;
        }

        // Same key signs licence tokens. Without this check one could be
        // presented where a trial token is expected.
        if (!KIND.equals(o.optString("k"))) return STATE_NONE;
        if (!MainActivity.PRODUCT_ID.equals(o.optString("pn"))) return STATE_NONE;

        // Gate 03 -- device binding. A token lifted off another phone dies here.
        String mine = deviceHash(c);
        if (mine == null || !mine.equals(o.optString("dev"))) return STATE_NONE;

        final long ends = o.optLong("ends", 0L);
        if (ends <= 0L) return STATE_NONE;

        // Gate 04 -- clock sanity before expiry. Once running out means the app
        // stops, winding the clock back is the highest-value attack there is and
        // it is two taps in Settings. A time below the highest we have ever seen
        // is not a valid "now", so the token is discarded rather than trusted --
        // which drops the device to capped sessions offline, and to the server's
        // answer online. Never to more than it had.
        final long now = System.currentTimeMillis() / 1000L;
        final long high = Activities.getDefaultsLong(PREF_HIGHWATER, 0L, c);
        if (now + 60L < high) return STATE_NONE;
        if (now > high) Activities.setDefaultsLong(PREF_HIGHWATER, now, c);

        if ("expired".equals(o.optString("verdict"))) return STATE_EXPIRED;
        return now >= ends ? STATE_EXPIRED : STATE_TRIAL;
    }

    // ------------------------------------------------------------- primitives

    static boolean verify(byte[] message, byte[] sig) {
        try {
            byte[] pub = b64(PUBLIC_KEY_B64);
            if (pub == null || pub.length != 32) return false;
            Ed25519Signer s = new Ed25519Signer();
            s.init(false, new Ed25519PublicKeyParameters(pub, 0));
            s.update(message, 0, message.length);
            return s.verifySignature(sig);
        } catch (Throwable t) {
            return false;
        }
    }

    /**
     * Base64 decode, accepting both the standard and URL-safe alphabets and
     * treating padding as optional.
     *
     * Hand-rolled rather than android.util.Base64 so the whole verification path
     * is plain Java and can be exercised by a JVM test. java.util.Base64 would do
     * but it needs API 26 and minSdk here is 24.
     */
    static byte[] b64(String in) {
        if (in == null) return null;
        int len = 0;
        int[] vals = new int[in.length()];
        for (int i = 0; i < in.length(); i++) {
            char ch = in.charAt(i);
            int v;
            if (ch >= 'A' && ch <= 'Z') v = ch - 'A';
            else if (ch >= 'a' && ch <= 'z') v = ch - 'a' + 26;
            else if (ch >= '0' && ch <= '9') v = ch - '0' + 52;
            else if (ch == '+' || ch == '-') v = 62;
            else if (ch == '/' || ch == '_') v = 63;
            else if (ch == '=') continue;
            else if (ch == '\r' || ch == '\n') continue;
            else return null;               // anything else is not base64
            vals[len++] = v;
        }
        if (len % 4 == 1) return null;      // impossible length
        byte[] out = new byte[len * 3 / 4];
        int oi = 0, buf = 0, bits = 0;
        for (int i = 0; i < len; i++) {
            buf = (buf << 6) | vals[i];
            bits += 6;
            if (bits >= 8) {
                bits -= 8;
                out[oi++] = (byte) ((buf >> bits) & 0xFF);
            }
        }
        return out;
    }

    // ------------------------------------------------------------------ mint

    /**
     * One round trip. Short timeouts on purpose: this runs off the UI thread, but
     * a server that accepts the connection and then hangs must not keep a launch
     * in limbo -- unreachable and slow resolve to the same thing.
     *
     * Anything that is not a 200 carrying a token we can parse is treated as
     * unreachable. That is deliberate and it is the lesson from the licence
     * client: a challenge page answers 200 with HTML, and a client that parses
     * optimistically grants unrestricted use in silence.
     */
    private static String mint(Context c) {
        String dev = deviceHash(c);
        if (dev == null) return null;

        HttpURLConnection conn = null;
        try {
            conn = (HttpsURLConnection) new URL(ENDPOINT).openConnection();
            conn.setRequestMethod("POST");
            conn.setConnectTimeout(5000);
            conn.setReadTimeout(7000);
            conn.setDoOutput(true);
            conn.setRequestProperty("Content-Type", "application/json");
            conn.setRequestProperty("Accept", "application/json");

            JSONObject body = new JSONObject();
            body.put("dev", dev);
            body.put("product", MainActivity.PRODUCT_ID);
            byte[] out = body.toString().getBytes(StandardCharsets.UTF_8);

            OutputStream os = conn.getOutputStream();
            os.write(out);
            os.close();

            if (conn.getResponseCode() != 200) return null;

            InputStream is = conn.getInputStream();
            ByteArrayOutputStream buf = new ByteArrayOutputStream();
            byte[] chunk = new byte[1024];
            int n, total = 0;
            while ((n = is.read(chunk)) > 0) {
                total += n;
                if (total > 8192) return null;   // a token is a few hundred bytes
                buf.write(chunk, 0, n);
            }
            is.close();

            String token = new JSONObject(buf.toString("UTF-8")).optString("token", "");
            return token.isEmpty() ? null : token;
        } catch (Throwable t) {
            // The exception type only, never the device hash, the token or the
            // body. Without this one line a mint that fails -- a blocked socket, a
            // DNS miss, an OEM background-network restriction on first launch --
            // is indistinguishable from a server that is simply down, and the
            // whole feature degrades to capped sessions with nothing to look at.
            Log.w("TrialGate", "mint failed: " + t.getClass().getSimpleName());
            return null;
        } finally {
            if (conn != null) conn.disconnect();
        }
    }
}
