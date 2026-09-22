#include <stdio.h>
#include <omp.h>

int main(void) {
#ifdef _OPENMP
    if (omp_get_max_threads() == 2) {
        return 0;
    } else {
        printf("Max threads is %d not 2.\n", omp_get_max_threads());
        return 1;
    }
#else
    printf("_OPENMP is not defined; is OpenMP compilation working?\n");
    return 1;
#endif
}
