<!-- Generated by zircon/scripts/update-docs-from-fidl, do not edit! -->
# zx_vmo_get_size

## SUMMARY

Read the current size of a VMO object.

## Declaration

```c
#include <zircon/syscalls.h>

zx_status_t zx_vmo_get_size(zx_handle_t handle, uint64_t* size);
```

## Description

`zx_vmo_get_size()` returns the current size of the [virtual memory
object](/docs/reference/kernel_objects/vm_object.md) (VMO).

## Rights

TODO(fxbug.dev/32253)

## Return value

`zx_vmo_get_size()` returns **ZX_OK** on success. In the event
of failure, a negative error value is returned.

## Errors

**ZX_ERR_BAD_HANDLE**  *handle* is not a valid handle.

**ZX_ERR_WRONG_TYPE**  *handle* is not a VMO handle.

**ZX_ERR_INVALID_ARGS**  *size* is an invalid pointer or NULL.

## See also

 - [`zx_vmo_create()`]
 - [`zx_vmo_create_child()`]
 - [`zx_vmo_op_range()`]
 - [`zx_vmo_read()`]
 - [`zx_vmo_set_size()`]
 - [`zx_vmo_write()`]

[`zx_vmo_create()`]: vmo_create.md
[`zx_vmo_create_child()`]: vmo_create_child.md
[`zx_vmo_op_range()`]: vmo_op_range.md
[`zx_vmo_read()`]: vmo_read.md
[`zx_vmo_set_size()`]: vmo_set_size.md
[`zx_vmo_write()`]: vmo_write.md
Read the current size of a VMO object.

