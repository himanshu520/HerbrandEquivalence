
# Herbrand Equivalence

LLVM implementation of Herbrand Equivalence algorithm mentioned [here](https://arxiv.org/abs/1708.04976 "A fix-point characterization of Herbrand equivalence of expressions in data flow frameworks").

## How to run

* First make sure you have [Clang](https://clang.llvm.org/ "Clang") installed. On linux it can be installed using `apt-get` command.
**Example** - Run `sudo apt-get install clang-8` to install **Clang version - 8**.

* Download and build [LLVM](https://llvm.org/ "LLVM Compiler Infrastructure") source code from its [download page](http://releases.llvm.org/download.html "LLVM Download Page"). Make sure that the versions of Clang and LLVM being used are compatible. Refer to [the documentation](https://llvm.org/docs/CMake.html "Building LLVM with CMake") for any help on building LLVM from source.

* Create a new LLVM pass, copy the code given in [src directory](./src "src directory"). Refer to [the documentation](http://llvm.org/docs/WritingAnLLVMPass.html "Writing an LLVM Pass") for any help on writing or running an LLVM pass.

## Commands Cheatsheet

* **Compiling a C source file using Clang**  
    `clang filename.c`  
    `clang filename.c -o filename`

* **Getting *LLVM bitcode* using Clang**  
    `clang -emit-llvm -c filename.c`  
    `clang -emit-llvm -c filename.c -o newname.bc`  
    The output of the first command is *filename.bc*

* **Getting *LLVM bitcode* in readable file format**  
    `clang -emit-llvm -s filename.c`  
    `clang -emit-llvm -s filename.c -o newname.ll`  
    The output of the first command is *filename.ll*

* **Converting a *ll file* to a *bc file***  
    `./bin/llvm-as filename.ll`  
    `./bin/llvm-as filename.ll -o newname.bc`  
    Assuming the command is run from the parent LLVM build directory.

* **Converting a *bc file* to a *ll file***  
    `./bin/llvm-dis filename.bc`  
    `./bin/llvm-dis filename.bc -o newname.ll`  
    Assuming the command is run from the parent LLVM build directory.

* **Directly executing *ll* and *bc* files**  
    `./bin/lli filename.bc`  
    `./bin/lli filename.ll`  
    Assuming the command is run from the parent LLVM build directory.

* **Running an LLVM pass on a C/C++ file**  
    First get the corresponding LLVM code in *ll* or *bc* format, then run the pass on it using **./bin/opt**. *Hello pass* is described in [the documentation](http://llvm.org/docs/WritingAnLLVMPass.html "Writing an LLVM Pass").  
    `clang -emit-llvm filename.c -o newname.bc`  
    `./bin/opt -load ./lib/LLVMHello.so -hello newname.bc -o newname_hello.bc`

## Repository structure

* **references** - Contains relevant papers  
* **reports** - Contains reports and presentations
* **src** - Contains the LLVM pass - implementation of the algorithm  
* **testcases** - Contains examples used for testing

## References

* [A fix-point characterization of Herbrand equivalence of expressions in data flow frameworks](https://arxiv.org/abs/1708.04976 "A fix-point characterization of Herbrand equivalence of expressions in data flow frameworks")
* [Clang](https://clang.llvm.org/ "Clang")
* [LLVM](https://llvm.org/ "LLVM Compiler Infrastructure")
* [Building LLVM with CMake](https://llvm.org/docs/CMake.html "Building LLVM with CMake")
* [Writing an LLVM Pass](http://llvm.org/docs/WritingAnLLVMPass.html "Writing an LLVM Pass")
