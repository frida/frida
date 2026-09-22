public class Person : Object {
    public string name {
        get {
            return "Joe Badger";
        }
    }

    public Beer favorite_beer {
        get;
        construct;
    }

    public Person() {
        Object(favorite_beer: new Beer("smooth"));
    }
}
