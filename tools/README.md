# Component Build

This repository contains the basic files and helper scripts to build VZLU software components and drivers.

## Contents

* `cppcheck/` - configuration files and scripts to perform static code scanning
* `meson/` - meson build system scripts and templates
* `vscode/` - configuration for Visual Studio Code to correctly set include paths
* `build.sh` - main build script, generates the configuration for meson and then builds the component via ninja build
* `clean.sh` - main clean script, removes all generated files and binaries

## Usage

This repository is intended to be included in all VZLU software drivers as a submodule. To build that software component, call the `build.sh` script:

```
./component-build/build.sh
```

This script will first generate the `meson.build` file from the component's `conf.meson.build` file and the common `base.meson.build` file in this repository. `conf.meson.build` contains the component-specific confuiguration (such as supported platforms), while `base.meson.build` contains common configuration of VZLU libraries. After `meson.build` is generated, it will then configure the component by using its meson options. Finally, the component is built into binaries based on that configuration, typically producing a client-side plugin, a server-side statically linked library, or automated self-test executable.
