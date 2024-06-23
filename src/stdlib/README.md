# Standard library

This is a custom superset of the C standard library, this is NOT POSIX, extensions can be found within the sys folder, this does potentially limit compatibility with some software.

## Naming convention

The conventions used in the extensions should be similar with the rest of the c library.
Internal functions should use _UppercaseCamelCase to comply with the spec.  
Internal defines don't need a special naming convention as they are not visible outside of the library.  
Auxiliary defines will be visible outside the library thus they should be _UPPERCASE_SNAKE_CASE to comply with the spec.  
