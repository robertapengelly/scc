## About

    This project provides a C preprocessor and a freestanding C90
    compiler with select extensions from later standards and
    compiler-specific extensions.
    
    See the `Language Support` section below.
    
    It is designed for Intel 386 and is intended for use in environments
    without a host operating system (e.g., kernels, embedded systems, or
    bare-metal hobbyist projects).
    
    Note: This is a freestanding implementation. It provides the C
          language translation but does not include a standard library,
          startup routines (crt0), or linker scripts. Users must
          provide these components to generate a functional executable.

## Limitations

    This project is currently in early development. Not all GNU
    extensions are supported. See the `Language Support` section
    below and `EXTENSIONS.md` for details.

## Language Support

    Preprocessing:
    
        Supports /* ... */ (block).
        
        If -pedantic is not specified, then
        // (single-line) comments are also supported.
        
        Supports #pragma once.
        
        Macro token pasting (##) and stringification (#) operators.
        
        If -pedantic is not specified, selected preprocessor
        extensions are enabled:
        
            `C99 Extensions`:
            
                Variadic macros (__VA_ARGS__).
    
    Compilation:
    
        All #pragma directives are currently ignored. There are
        no immediate plans to implement specific pragma support,
        but they are reserved for future compiler-specific features.
        
        Standard C90: Core language features and scope rules.
        
        By default, selected extensions beyond C90 are enabled:
        
            `C99 Extensions`:
            
                inline
                restrict
                long long
            
        `Compiler-Specific Extensions`:
        
            If -pedantic is not specified, then support for selected
            compiler extensions.
            
            See `EXTENSIONS.md` for a list of supported features.
            
            Support for 64-bit long types on 32-bit targets via the
            -mlong64 flag.
            
                Note:   When building with a 32-bit host compiler or MSVC,
                        a bootstrap build is recommended as scc uses
                        long in various places.
        
        Scoping: Strict C90—all local variable declarations must appear at
        the beginning of a block.
        
        Wide character constants (L'a') and wide string literals (L"...")
        are represented as 16-bit values. Define wchar_t as a compatible
        16-bit type (e.g., unsigned short). Programs that define wchar_t
        incompatibly will have implementation-defined or incorrect
        behaviour.

## Usage

    To preprocess the input:
        scc -E -o example.i example.c
    
    To transform the input to assembly:
        scc -S -o example.s example.c
    
    To transform the input to an object:
        scc -o example.o example.c
    
    The macro __STDC__ is defined when -std=c90
    is specified.
    

## License

    All source code is Public Domain.

## Obtain the source code

    git clone https://git.candlhat.org/scc.git

## Building

    BSD:
    
        Make sure you have gcc and gmake installed then run gmake -f Makefile.unix.
    
    Linux:
    
        Make sure you have gcc and make installed then run make -f Makefile.unix.
    
    macOS:
    
        Make sure you have xcode command line tools installed then run
        make -f Makefile.unix.
    
    Windows:
    
        Make sure you have mingw installed and the location within your PATH variable
        then run mingw32-make.exe -f Makefile.w32.
