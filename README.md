# cheatcpp — CLI Cheatsheets (C++ port of cheat)

A zero-dependency C++ port of [cheat](https://github.com/cheat/cheat) — create, view, and search interactive cheatsheets for command-line tools.

## Why cheatcpp?

The original [cheat](https://github.com/cheat/cheat) requires Go plus dozens of modules. cheatcpp compiles with a single `make` using only C++17.

## Quick Start

```bash
make
./cheatcpp init                # Initialize cheatsheet directory
./cheatcpp tar                 # View the "tar" cheatsheet
./cheatcpp -l                  # List all cheatsheets
./cheatcpp -s "git log"        # Search for phrase
./cheatcpp -e mytool           # Edit/create cheatsheet
```

## Features

- **view** — Display a cheatsheet with optional colorization and pager
- **list** — List all available cheatsheets (`-b` for titles only)
- **search** — Search cheatsheet contents for a phrase (`-r` for regex)
- **edit** — Edit or create a cheatsheet (opens `$EDITOR`)
- **tags** — List all tags in use; filter by tag (`-t`)
- **dirs** — Show cheatsheet directory paths
- **conf** — Show config file path
- **remove** — Delete a cheatsheet
- **init** — Initialize cheatsheet directory structure

## Build

```bash
make
```
Requires: GCC 10+ or Clang 12+, GNU Make
