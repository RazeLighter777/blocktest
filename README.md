# blocktest
messing with opengl

# Commands

```bash
conan profile detect --force
```

```bash
conan install . --output-folder=build --build=missing
```

```bash
cd build/
cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build .
 ```