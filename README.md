### tlang
#### Language Project - CSCI 4900

  Requires [LLVM](https://www.llvm.org)
  
On Ubuntu:
```bash
sudo apt-get install llvm
```
On Arch:
```bash
sudo pacman -S llvm
```
  
  To compile:
  
```bash
clang++ -g tlang.cpp `llvm-config --cxxflags --ldflags --system-libs --libs core` -o tlang
```
