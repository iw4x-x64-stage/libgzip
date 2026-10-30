# libgzip - Gzip compression for IW4x projects.

The `libgzip` library compresses and decompresses data in the gzip file
format that RFC 1952 specifies, with the deflate compression of the
`libdeflate` library.

## Usage

To start using `libgzip` in your project, add the following `depends`
value to your `manifest`, adjusting the version constraint as appropriate:

```
depends: libgzip ^0.1.0
```

Then import the library in your `buildfile`:

```
import libs = libgzip%lib{gzip}
```

## Development

The development setup for `libgzip` uses the standard `bdep`-based
workflow. On Linux we build natively with GCC and also cross-compile for
Windows with MinGW-w64 GCC, running the tests under Wine with `WINEPATH`
pointing to the directory that contains the MinGW runtime DLLs
(`/usr/x86_64-w64-mingw32/sys-root/mingw/bin` on Fedora). For example:

```
git clone https://github.com/iw4x-x64-stage/libgzip.git
cd libgzip

bdep init -C @gcc cc config.cxx=g++
bdep init -C @mingw cc config.cxx=x86_64-w64-mingw32-g++ \
  config.test.runner="env WINEPATH=/usr/x86_64-w64-mingw32/sys-root/mingw/bin wine"
bdep update -a
bdep test -a
```

On Windows we build with MSVC from the x64 Native Tools Command Prompt or the
matching PowerShell, which builds 64-bit binaries. For example:

```
git clone https://github.com/iw4x-x64-stage/libgzip.git
cd libgzip

bdep init -C -@msvc cc config.cxx=cl
bdep update
bdep test
```

## Contributing

See https://github.com/iw4x-x64-stage/.github/blob/main/CONTRIBUTING.md

## License

`libgzip` is licensed under the GNU General Public License, version 3,
subject to the additional permissions described in version 1.1 of the IW4x
Linking Exception.

See [LICENSE](LICENSE.md), [LICENSE-EXCEPTION](LICENSE-EXCEPTION.md), and
[AUTHORS](AUTHORS).
