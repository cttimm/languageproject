### tlang
#### Language Project - CSCI 4900

  Requires [LLVM](https://www.llvm.org)
  
  *Requires the latest version of llvm, which had to be compiled manually
  
```bash
git clone http://www.github.com/llvm-mirror/llvm
```
  
  To compile:
  
```bash
clang++ -g tlang.cpp `llvm-config --cxxflags --ldflags --system-libs --libs core mcjit native` -O3 -o tlang
```


