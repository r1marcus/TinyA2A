"""Python setup for tinya2a bindings.

Note: this packages only the Python wrapper. The underlying C library
(libtinya2a.{so,dylib}) is loaded via ctypes at import time. Build it
separately with `make` or `cmake` in the parent directory before installing,
or set TINYA2A_LIB_PATH to point at a prebuilt shared library.
"""
from setuptools import setup, find_packages
from pathlib import Path

readme = (Path(__file__).resolve().parent.parent / "README.md")
long_desc = readme.read_text() if readme.exists() else ""

setup(
    name="tinya2a",
    url="https://github.com/r1marcus/TinyA2A",
    version="0.1.0",
    description="Python bindings for the TinyA2A and A2A-Z reference C library",
    long_description=long_desc,
    long_description_content_type="text/markdown",
    author="Marcus Rüb"
    , author_email="m.rueb@foresthub.ai",
    license="Apache-2.0",
    packages=find_packages(),
    python_requires=">=3.7",
    classifiers=[
        "License :: OSI Approved :: Apache Software License",
        "Programming Language :: Python :: 3",
        "Topic :: Communications",
        "Topic :: System :: Distributed Computing",
        "Intended Audience :: Developers",
    ],
)
