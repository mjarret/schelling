# distutils: language=c++
# cython: language_level=3, boundscheck=False, wraparound=False

# Minimal Cython wrapper to benchmark the Python Schelling process function.
# This does not rewrite the algorithm, it only reduces Python loop overhead
# around repeated calls for a closer apples-to-apples with the C++ bench.

cimport cython
import time as _pytime
import py_api  # same directory import (Python_Version)

@cython.cfunc
cdef double _now() noexcept:
    return <double>_pytime.perf_counter()

@cython.boundscheck(False)
@cython.wraparound(False)
cpdef double ms_per_call(int cs, int pl, int reps=5):
    cdef int i
    cdef double t0 = _now()
    for i in range(reps):
        py_api.run_schelling_process_py(cs, pl)
    cdef double t1 = _now()
    return 1e3 * (t1 - t0) / reps

