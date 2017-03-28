###tlang
#### Language Project - CSCI 4900

  Requires LLVM
  
  To compile:
  
```bash
clang++ -g tlang.cpp `llvm-config --cxxflags --ldflags --system-libs --libs core` -o tlang
```
