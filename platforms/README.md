# platforms

One directory per target. Each holds that target's build inputs, so adding a
platform adds a directory and does not move anything.

| Path                          | Status   | Purpose                                                                     |
| ----------------------------- | -------- | --------------------------------------------------------------------------- |
| `<name>/config.cmake`         | required | The `cmake -C` file: `PORT`, tick rate, heap, `SOURCES`, `LINKER_SCRIPT`    |
| `<name>/startup.c`            | required | Startup code and vector table (a placeholder on hosted targets)             |
| `<name>/link.ld`              | required | Linker script                                                               |
| `<name>/storage/`             | optional | littlefs driver defining `OS_BSP_StorageConfig`, chosen with `OSAL_FREERTOS_STORAGE_TARGET` |

Configure a platform with `cmake -S . -B build -C platforms/<name>/config.cmake`.
The keys `config.cmake` takes are documented in
`generic-freertos/config/osal_freertos_platform.cmake`.

Tests sit beside this, not inside it: white-box tests in `freertos-coverage-tests/`,
black-box tests from OSAL's own `src/tests/`.
