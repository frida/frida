namespace App {
    public static int main(string[] args) {
        var person = new Person();
        print("Favorite beer of \"%s\" is %s\n", person.name, person.favorite_beer.flavor);

        var beer = new Beer("tasty");
        print("This beer is %s\n", beer.flavor);

        return 0;
    }
}
