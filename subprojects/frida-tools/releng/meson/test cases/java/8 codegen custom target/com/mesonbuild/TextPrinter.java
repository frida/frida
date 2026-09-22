package com.mesonbuild;

class TextPrinter {

    private String msg;

    TextPrinter(String s) {
        msg = s;
    }

    public void print() {
        System.out.println(msg);
    }
}
