package com.mid.memtest;

import android.util.SparseArray;



public class MemtestParser {
    public static SparseArray<String > msg = new SparseArray<>();
    static  {
        msg.append(1<<1, "Stuck Address");
        msg.append(1<<2, "Random Value");
        msg.append(1<<3, "Compare XOR");
        msg.append(1<<4, "Compare SUB");
        msg.append(1<<5, "Compare MUL");
        msg.append(1<<6, "Compare DIV");
        msg.append(1<<7, "Compare OR");
        msg.append(1<<8, "Compare AND");
        msg.append(1<<9, "Sequential Increment");
        msg.append(1<<10, "Solid Bits");
        msg.append(1<<11, "Block Sequential");
        msg.append(1<<12, "Checkerboard");
        msg.append(1<<13, "Bit Spread");
        msg.append(1<<14, "Bit Flip");
        msg.append(1<<15, "Walking Ones");
        msg.append(1<<16, "Walking Zeroes");
    }



    public static String parse(int code){
        StringBuilder builder = new StringBuilder();
        for(int i=0; i<msg.size(); i++){
            int key = msg.keyAt(i);
            String value = msg.valueAt(i);
            builder.append(String.format("%-20s: %s\n", value, (code & key) == 0 ? "OK":"FAIL"));
        }
        builder.append(String.format("%-20s: %s\n", "Result:", code == 0 ? "OK":"FAIL"));
        return builder.toString();
    }
}
