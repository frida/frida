using System;

namespace Abc
{
    public static class Something
    {
        public static bool Api1(this String str)
        {
            return str == "foo";
        }
    }
}
