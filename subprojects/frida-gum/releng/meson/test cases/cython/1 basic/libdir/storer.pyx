cimport cstorer

cdef class Storer:
    cdef cstorer.Storer* _c_storer

    def __cinit__(self):
        self._c_storer = cstorer.storer_new()

    def __dealloc__(self):
        cstorer.storer_destroy(self._c_storer)

    cpdef int get_value(self):
        return cstorer.storer_get_value(self._c_storer)

    cpdef set_value(self, int value):
        cstorer.storer_set_value(self._c_storer, value)
