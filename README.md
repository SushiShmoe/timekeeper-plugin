# Component Driver Template
This repository contains the template project to be used for implementation of drivers for various flight hardware procured from 3rd parties, or internally developed - reaction wheels, gyroscopes, or full subsystems.

Rationale is to create a unified driver structure that enables various levels of hardware- and software-in-the-loop testing.

For further details, see [the Doxygen main page](doc/doxygen_mainpage.md).

## How to use

Usually, the easiest/fastest option is to fork this repository. When the target repository is to be hosted somewhere other than GitLab.com, then the workflow is as follows:

```
$ git clone git@gitlab.com:vzlu/sw/component-template.git <your-component-name>
$ git remote rename origin template
$ git remote add origin <path-to-your-origin>
$ git push -u origin --all
$ git push -u origin --tags
```

After this, you can use this template for your SW driver implementation. Usually this will involve:

1. Updating the configuration file (conf.meson.build) to fit your target platform, backends and source files. The numbered steps in the file will help you get started, however, knowledge of the build system is assumed.
2. Adding source files for client, server, autotests, commands etc.
3. Building and testing your client/server or deploying command plugins.

## Build

The simplest way to build all the different configurations is to run the recipes from the `Makefile`. The fastest test to check everything builds correctly is `make test`, which will build and execute a complete test of client and server communicating through an emulated COM port. To build the CSP version of client and server, use `make csp` and to test it, run `make csp-test`.

The following dependencies are needed:

* meson >=0.61.2 (can be installed with `pip install meson` - the version in apt packages is usually outdated!)
* ninja, gcc and make
* submodules to this repository, which can be downloaded with `git submodule update --init`
* cargo (to build lpldgen, if it is not available as shared library)

In addition there are the following VZLU dependencies, which are handled automatically via meson subprojects. If you do not have them installed as sahred library (e.g. because you are developing VCOM plugins), they will be downloaded locally for this project only.

* lpldgen (payload parser/generator): https://gitlab.com/vzlu/sw/lpldgen
* libcsp (VZLU fork): https://gitlab.com/vzlu/sw/libcsp
* SALLY (abstraction layer for software services): https://gitlab.com/vzlu/sw/sally

## Contents
* `autotest/` - contains source files for autoamted tests. These are also useful as simple examples, as each automated test needs to have everything needed to execute the actual software component.
* `build/` - build outputs and executable binaries
* `cli/` - client-side source files and headers
* `common/` - source files and headers shared between client and server (typically packet structure definitions)
* `dev/` - emulated COM ports
* `doc/` - Doxygen-generated documentation
* `lib/` - libraries which are not meson subprojects
* `scripts/` - simple helper scripts for building, cleaning and crating emulated COM ports
* `srv/` - server-side source files and headers
* `subprojects/` - meson subprojects (managed dependencies)
* `tools/` - submodule which contains helper scripts for building and static code analysis

## Documentation

To generate the documentation for this repository, go to the [doc/generated](doc/generated) folder and run `doxygen`

The documentation will be accessible through the .html files that were generated.
