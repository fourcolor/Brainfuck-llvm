# Brainfuck-llvm
## Requirement
llvm-12
## Usage
```
$ clang++-12 `llvm-config-12 --cxxflags --ldflags --system-libs --libs core`  brainfuck.cpp -o b20.o

$ ./b20.o < test2

$ g++ output.o

$ ./a.out
```
