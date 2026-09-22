
auto secondModulePeopleVersionSet ()
{
    version (With_People) {
        return true;
    } else {
        return false;
    }
}
