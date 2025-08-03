# Contributing to PatchworkOS

Thank you for wanting to contribute to PatchworkOS!

Included here are instructions on how to contribute, try to follow them as best you can but dont sweat it if you make mistakes. (These instructions are currently WIP)

## Commit Messages

Follow the [conventional commit format](https://www.conventionalcommits.org/en/v1.0.0/) used throughout the project:
```
type(scope): description
```

For example:
- `fix(readme): fix typo in setup instructions`
- `feat(kernel:sched): add constant-time scheduler`
- `refactor(libstd): reorganize memory functions`

Note the use of lowercase and present tense ("add" not "added").

## Code Conventions

If you submit code, then follow these conventions.

### Names

- For variables use `camelCase`.
- For functions use `snake_case`.
- For macros and constants use `SCREAMING_SNAKE_CASE`.
- All internal functions, variables, macros or constants in libstd must be prefixed with `_` like `_my_function()`, `_myVariable` or `_MY_CONSTANT` to avoid conflicts.

### Formatting

Follow the formatting outlined in the [.clang-format](https://github.com/KaiNorberg/PatchworkOS/blob/main/.clang-format) file. 

Applying formatting is as simple as running `make format`, always run this before pushing a commit.

### Rules

- In if statements, be explicit, write `ptr == NULL` not `!ptr`. Booleans are an exception (`if (flag)` is fine).

## Questions?

Feel free to open an issue if you have questions about contributing!
