// No includes here, they need to come from the PCH

void func(void) {
    fprintf(stdout, "This is a function that fails if stdio is not #included.\n");
}

int main(void) {
    return 0;
}
