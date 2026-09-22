package com.mesonbuild;

class Simple {
    class Inner {
        public String getString() {
            return "Inner class is working.\n";
        }
    }

    public static void main(String [] args) {
        Simple s = new Simple();
        Simple.Inner ic = s.new Inner();
        System.out.println(ic.getString());
    }
}
