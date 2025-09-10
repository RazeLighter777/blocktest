# blocktest
messing with opengl

# Commands

```bash
conan profile detect --force
```

```bash
conan install .
```

```bash
cd build/
cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build .
 ```