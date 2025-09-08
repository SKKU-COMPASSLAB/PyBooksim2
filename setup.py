import os
import logging
import subprocess
from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext
from Cython.Build import cythonize

PROJECT_ROOT_DIR    = os.path.abspath(os.path.dirname(__file__))
BOOKSIM_SRC_DIR     = os.path.join(PROJECT_ROOT_DIR, "src")
PYBOOKSIM_ROOT_DIR  = os.path.join(PROJECT_ROOT_DIR, "pybooksim2")
PYBOOKSIM_LIB_DIR   = os.path.join(PYBOOKSIM_ROOT_DIR, "lib")
PYBOOKSIM_LIB_OBJ   = os.path.join(PYBOOKSIM_LIB_DIR, "libbooksim2.a")

def build_booksim2():
    logging.info("Building DRAMSim3...")
    
    cmake_args = ["cmake", "-S", PROJECT_ROOT_DIR, "-B", PYBOOKSIM_ROOT_DIR]
    build_args = ["cmake", "--build", PYBOOKSIM_ROOT_DIR]
    
    logging.info("Running: %s", " ".join(cmake_args))
    subprocess.check_call(cmake_args)
    
    logging.info("Running: %s", " ".join(build_args))
    subprocess.check_call(build_args)
    
class BuildBookSim2Extension(build_ext):
    def run(self):
        build_booksim2()
        
        return super().run()

ext = Extension(
    "pybooksim2.pybooksim2",
    sources=["pybooksim2/pybooksim2.pyx"],
    include_dirs=[BOOKSIM_SRC_DIR],
    language="c++",
    extra_compile_args=["-O3", "-Wl,--no-undefined"],
    extra_objects=[PYBOOKSIM_LIB_OBJ],
)

setup(
    name="pybooksim2",
    version="0.1",
    description='Python extension for the DRAMSim3',
    author='Seongwook Kim',
    author_email='su8939@skku.edu',
    packages=["pybooksim2"],
    package_data={"pybooksim2": ["pybooksim2.pyi", "py.typed"]},
    ext_modules=cythonize(
        [ext], 
        annotate=True, 
        compiler_directives={
            "annotation_typing": True,
        },
        language_level=3),
    include_package_data=True,
    zip_safe=False,
    cmdclass={"build_ext": BuildBookSim2Extension},
)
