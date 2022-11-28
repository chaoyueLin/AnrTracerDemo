package com.example.tracer;

public class MatrixLog {

    public static void e(final String tag, final String format, final Object... params) {
        String log = (params == null || params.length == 0) ? format : String.format(format, params);
        android.util.Log.e(tag, log);
    }


    public static void i(final String tag, final String format, final Object... params) {
        String log = (params == null || params.length == 0) ? format : String.format(format, params);
        android.util.Log.i(tag, log);

    }
}
