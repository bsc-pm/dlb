README
======

This example provides a minimal CMake Project for both Fortran and C code examples.
In order to use the CMake integration, please make sure to add the installation path of DLB to the `CMAKE_PREFIX_PATH` variable.

Assuming the installation of DLB is located at `<DLB_PREFIX>` we can configure the project like

``` bash
export CMAKE_PREFIX_PATH+=":<DLB_PREFIX>"
mkdir build
cd build
cmake ..
make
```

Doing this should compile the binaries `appc` and `appf`
