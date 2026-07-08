# SALLY Backends

This repository contains all backends for the [SALLY abstraction layer](https://gitlab.com/vzlu/sw/sally). The goal is to have a single source for all backends (e.g. the FatFS filesystem backend, the FreeRTOS OS backend etc.).

## How to use
1. Add this repository to your project as a submodule.
2. Select which backends you would like to add to your project. This can be done by directly setting the CMake source.
3. Use the backend as `extern`.

## Example
Project master CMakeLists.txt (sally-backends is a submodule in `lib/`):
```
file(GLOB_RECURSE SOURCES
  "lib/sally-backends/fs/fatfs_backend.c"
  "lib/sally-backends/os/cmsis_backend.c"
)
```

Setting up OS and FS backends during startup:
```c
#include <service/service_os.h>

void startup() {
  (...)

  // set the OS backend
  extern service_os_t cmsis;
  service_set_os(&cmsis);

  // set the FS backend
  extern service_os_t backend_fs_ff;
  backend_fs_ff.total_size = DATA_MEMORY_SIZE;

  (...)
}
```
