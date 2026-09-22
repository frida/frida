using System;
using System.Resources;

public class Prog {

    static public void Main () {
        ResourceManager res = new ResourceManager(typeof(TestRes));
        Console.WriteLine(res.GetString("message"));
    }

    internal class TestRes {
    }
}
