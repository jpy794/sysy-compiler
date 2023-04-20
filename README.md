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

#### Docker

**Download Image**

```shell
docker pull archlinux
```
**Create Container**

```shell
docker create --name NAME -v "AbsolutePath:/sysy-compiler" -it archlinux
```
**Access Container Shell**

```shell
docker start -ai NAME
```
**Download Required Packages**

```shell
pacman -Syu
pacman -S antlr4 antlr4-runtime #choose default as the provider for java-environment
pacman -S git cmake make gcc
```
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