# libgzip - A C++ library

The `libgzip` C++ library provides <SUMMARY-OF-FUNCTIONALITY>.


## Usage

To start using `libgzip` in your project, add the following `depends`
value to your `manifest`, adjusting the version constraint as appropriate:

```
depends: libgzip ^<VERSION>
```

Then import the library in your `buildfile`:

```
import libs = libgzip%lib{<TARGET>}
```


## Importable targets

This package provides the following importable targets:

```
lib{<TARGET>}
```

<DESCRIPTION-OF-IMPORTABLE-TARGETS>


## Configuration variables

This package provides the following configuration variables:

```
[bool] config.libgzip.<VARIABLE> ?= false
```

<DESCRIPTION-OF-CONFIG-VARIABLES>
