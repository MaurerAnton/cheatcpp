# cheatcpp — CLI Cheatsheets (C++ port of cheat)

A zero-dependency C++ port of [cheat](https://github.com/cheat/cheat) — create and view interactive cheatsheets for command-line tools.

## Why cheatcpp?

The original [cheat](https://github.com/cheat/cheat) requires the Go toolchain plus dozens of modules. cheatcpp compiles with a single `make` using only C++17 and standard Linux headers.

## Quick Start

```bash
make
./cheatcpp tar
```

## Features

- Search and display community cheatsheets
- Create personal cheatsheets
- Edit cheatsheets inline
- Tag-based organization
- Colored, formatted output
- Fuzzy search

## Build

```bash
make
```
Requires: GCC 10+ or Clang 12+, GNU Make
