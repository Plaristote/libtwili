# libtwili

libtwili is a C++ header inspector. It takes a list of headers or directories as input, and
uses libclang to extract lists of namespaces, classes, methods and functions.

# Build

libtwili uses the [build2](https://www.build2.org) build system. If you haven't already,
use the [install guide](https://www.build2.org/install.xhtml)
to get yourself prepared for what comes next.

The following commands will clone this repository and build the library:

```sh
git clone https://github.com/Plaristote/libtwili.git

bpkg create -d "build-gcc" cc config.cxx=g++

cd build-gcc

bpkg add --type dir ../libtwili
bpkg fetch
bpkg build libtwili '?sys:libclang/*'
```

You can then install libtwili to your system with the following command:

```sh
bpkg install libtwili config.install.root=$INSTALL_DIRECTORY
```
Replace `$INSTALL_DIRECTORY` with the your actual install directory name.

If you need `sudo` to install libraries in your install directory, use
the `config.install.sudo` option:

```sh
bpkg install libtwili config.install.root=/usr config.install.sudo=sudo
```

# Usage

TODO
