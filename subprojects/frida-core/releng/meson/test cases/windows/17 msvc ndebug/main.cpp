int main() {
#ifdef NDEBUG
    // NDEBUG is defined
    return 0;
#else
    // NDEBUG is not defined
    return 1;
#endif
}