trait Foo {
    fn foo(&self, Box<dyn Foo>);
}
