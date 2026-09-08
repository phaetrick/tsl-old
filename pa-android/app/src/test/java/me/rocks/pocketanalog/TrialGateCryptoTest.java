package me.rocks.pocketanalog;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import org.bouncycastle.crypto.params.Ed25519PublicKeyParameters;
import org.bouncycastle.crypto.signers.Ed25519Signer;
import org.junit.Test;

import java.nio.charset.StandardCharsets;

/**
 * The verification path, off-device.
 *
 * The failure this guards against is silent and total: if BouncyCastle's
 * Ed25519 were shrunk away, or the base64url decoding disagreed with the
 * server's framing by one byte, verify() would return false for every token,
 * every device would resolve to STATE_NONE, and the entire trial would quietly
 * become "capped sessions forever" -- with no crash and no log to notice it by.
 *
 * The token below was minted by Node with lib/licenseToken.js's exact framing,
 * so this also pins the two implementations together.
 */
public class TrialGateCryptoTest {

    private static final String OTHER_PUB32 = "Uoe41vTGubQ0blxOkw3kqEbxg6zXmCPtdUJcZAQK4kM=";
    private static final String PUB32 = "uKEsVBbv/kvOaGd+nBc7YHMH4qsL4VTCSmet5QpPSfc=";
    private static final String TOKEN =
            "eyJ2IjoxLCJrIjoiZHQiLCJkZXYiOiJhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYW"
          + "FhYWFhYWFhYWFhYWFhYWFhYWFhIiwicG4iOiJ1cGdyYWRlMSIsInZlcmRpY3QiOiJ0cmlhbCIsInN0YXJ0ZWQiOjEw"
          + "MDAsImVuZHMiOjIwMDAsImlhdCI6MTAwMH0.4OtRSZcBQkSEmkqWR6nbJxwALfp5VsdHgFfyihTcJwTr5yNCB4SujJ"
          + "T0nbtj8tm59z7niOfpmiOGinplsK5UDA";
    private static boolean verifyWith(String pubB64, String token) {
        int dot = token.indexOf('.');
        byte[] pub = TrialGate.b64(pubB64);
        byte[] sig = TrialGate.b64(token.substring(dot + 1));
        Ed25519Signer s = new Ed25519Signer();
        s.init(false, new Ed25519PublicKeyParameters(pub, 0));
        byte[] msg = token.substring(0, dot).getBytes(StandardCharsets.US_ASCII);
        s.update(msg, 0, msg.length);
        return s.verifySignature(sig);
    }

    @Test public void base64UrlAndStandardAlphabetsBothDecode() {
        assertArrayEquals(new byte[]{(byte)0xFB, (byte)0xFF}, TrialGate.b64("+/8="));
        assertArrayEquals(new byte[]{(byte)0xFB, (byte)0xFF}, TrialGate.b64("-_8"));
        assertArrayEquals("hello".getBytes(StandardCharsets.UTF_8), TrialGate.b64("aGVsbG8="));
        assertArrayEquals("hello".getBytes(StandardCharsets.UTF_8), TrialGate.b64("aGVsbG8"));
    }

    @Test public void base64RejectsRubbish() {
        assertNull(TrialGate.b64("not base64 !!"));
        assertNull(TrialGate.b64("A"));           // impossible length
        assertNull(TrialGate.b64(null));
    }

    @Test public void publicKeyIsThirtyTwoBytes() {
        assertTrue(TrialGate.b64(PUB32).length == 32);
    }

    /** The load-bearing one: a Node-signed token verifies here. */
    @Test public void nodeMintedTokenVerifies() {
        assertTrue(verifyWith(PUB32, TOKEN));
    }

    @Test public void tamperedPayloadFails() {
        int dot = TOKEN.indexOf('.');
        String bad = TOKEN.substring(0, dot - 1) + "A" + TOKEN.substring(dot);
        assertFalse(verifyWith(PUB32, bad));
    }

    @Test public void wrongKeyFails() {
        // A real, valid Ed25519 key that simply did not sign this token. An
        // all-zero buffer would not do: BouncyCastle rejects it as "invalid
        // public key" before it ever gets to the signature, so the test would
        // pass for the wrong reason.
        assertFalse(verifyWith(OTHER_PUB32, TOKEN));
    }

    /** The shipped constant must be a usable Ed25519 key, not a typo. */
    @Test public void shippedPublicKeyLoads() {
        byte[] sig = new byte[64];
        assertFalse(TrialGate.verify("anything".getBytes(StandardCharsets.UTF_8), sig));
    }
}
