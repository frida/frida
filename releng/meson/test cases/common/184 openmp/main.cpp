#include <iostream>
#include <omp.h>

int main(void) {
#ifdef _OPENMP
    if (omp_get_max_threads() == 2) {
        return 0;
    } else {
        std::cout << "Max threads is " << omp_get_max_threads() << " not 2." << std::endl;
        return 1;
    }
#else
    printf("_OPENMP is not defined; is OpenMP compilation working?\n");
    return 1;
#endif
}
