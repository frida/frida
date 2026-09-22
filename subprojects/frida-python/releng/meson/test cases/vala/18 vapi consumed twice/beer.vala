public class Beer : Object {
    public string flavor {
        get;
        construct;
    }

    public Beer(string flavor) {
        Object(flavor: flavor);
    }
}
