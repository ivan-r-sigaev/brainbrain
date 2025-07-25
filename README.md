# brainbrain

[![License](https://img.shields.io/badge/License-MIT-yellow.svg)](./LICENSE)

A brainf*ck compiler.

## Features

- Compiles brainf*ck to NASM
- Currently supported targets: linux, libc

## Usage

1. Clone the repository:
```bash
git clone https://github.com/ivan-r-sigaev/brainbrain.git
```

2. Build the project:
```bash
cd brainbrain
cmake -B ./build/
cmake --build ./build/
```

3. Run the compiler:
```bash
cd ..
./build/brainbrain ./examples/hello.bf
```
