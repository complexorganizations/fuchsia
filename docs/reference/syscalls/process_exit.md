<!-- Generated by zircon/scripts/update-docs-from-fidl, do not edit! -->
# zx_process_exit

## Summary

Exits the currently running process.

## Declaration

```c
#include <zircon/syscalls.h>

[[noreturn]] void zx_process_exit(int64_t retcode);
```

## Description

The `zx_process_exit()` call ends the calling process with the given
return code. The return code of a process can be queried via the
**ZX_INFO_PROCESS** request to [`zx_object_get_info()`].

## Rights

None.

## Return value

`zx_process_exit()` does not return.

## Errors

`zx_process_exit()` cannot fail.

## See also

 - [`zx_object_get_info()`]
 - [`zx_process_create()`]

[`zx_object_get_info()`]: object_get_info.md
[`zx_process_create()`]: process_create.md

