using System;
using GLib;

public class Prog {
    static public void Main (string[] args) {
        Console.WriteLine(GLib.FileUtils.GetFileContents(args[0]));
    }
}
