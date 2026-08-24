## Supported Extensions

This compiler supports a subset of compiler-specific extensions.
These extensions are available when `-pedantic` is not specified.
    
### `__asm__` (Inline Assembly)

The __asm__ keyword allows the insertion of raw assembly
instructions into the output file.

See `__asm__` Inline Assembly Syntax below for more details.

### `__inline__`
A synonym for `inline`.

### `__restrict__`
A synonym for `restrict`.

## Pragma Directives

All `#pragma` directives are currently ignored by the compiler. They are
reserved for future compiler-specific features.

## `__asm__` Inline Assembly Syntax

This directive supports GNU-style extended inline assembly, including
output operands, input operands, read/write operands, register and
memory constraints, `volatile`, and clobber lists.

As Intel syntax is the primary target, arguments are written
left-to-right rather than right-to-left.

Registers should begin with `%%`, as a single `%` specifies an
argument index to inject.

`outb` and `outw` are special cases and should use operands in their
natural left-to-right order.

Usage examples:

```c
__asm__ ("mov %%eax, %0" : : "N" (port));
__asm__ ("outb %0, %1" : : "a" (val), "d" (port));
