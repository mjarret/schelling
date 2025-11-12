from setuptools import setup, Extension
from Cython.Build import cythonize

extensions = [
    Extension(
        name="cython_bench",
        sources=["cython_bench.pyx"],
        language="c++",
    )
]

setup(
    name="cython_bench",
    ext_modules=cythonize(extensions, compiler_directives={
        "language_level": 3,
        "boundscheck": False,
        "wraparound": False,
    }),
)

