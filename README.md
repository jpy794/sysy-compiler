# SysY Compiler

## Build

### Dependencies

- antlr4 & antlr4 cpp runtime

#### Archlinux

```
# pacman -S antlr4 antlr4-runtime
```

#### Ubuntu >= 22.10

```
# apt install antlr4 libantlr4-runtime-dev
```

Then download corresponding version of `antlr-complete.jar` for antlr-runtime manually and build with `cmake -DANTLR4_JAR_LOCATION=/path/to/antlr-complete.jar`

### How To Build

Here is an example, which will
- Build for debug
- Generate `compile_commands.json` for clangd

```
$ mkdir build && cd build
$ cmake -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=TRUE \
        ..
$ make -j`nproc`
```