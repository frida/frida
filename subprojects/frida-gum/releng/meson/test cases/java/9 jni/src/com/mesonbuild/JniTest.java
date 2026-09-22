package com.mesonbuild;

public final class JniTest {
    private static native int jni_test();

    public static void main(String[] args) {
        if (jni_test() != Configured.FINGERPRINT) {
            throw new RuntimeException("jdk_test() did not return 0");
        }
    }

    static {
        System.loadLibrary("jnijava");
    }
}
