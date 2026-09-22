// No includes here, they need to come from the PCH or explicit inclusion

void func(void) {
    fprintf(stdout, "This is a function that fails if stdio is not #included.\n");
    setlocale(LC_ALL, ""); /* This will fail if locale.h is not included */
}

int main(void) {
    return 0;
}
