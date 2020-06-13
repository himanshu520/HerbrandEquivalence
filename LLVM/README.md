
# Herbrand Equivalence Analysis

LLVM implementation of Herbrand Equivalence analysis algorithm mentioned [here](https://arxiv.org/abs/1708.04976 "A fix-point characterization of Herbrand equivalence of expressions in data flow frameworks"). Doxygen generated documentation can be found [here](https://himanshu520.github.io/HerbrandEquivalenceLLVMDocs/ "HerbrandEquivalenceLLVMDocs").

## How to run

* First make sure you have [Clang](https://clang.llvm.org/ "Clang") installed. On linux it can be installed using `apt-get` command.
**Example** - Run `sudo apt-get install clang-8` to install **Clang version - 8**.

* Download and build [LLVM](https://llvm.org/ "LLVM Compiler Infrastructure") source code from its [download page](http://releases.llvm.org/download.html "LLVM Download Page"). Make sure that the versions of Clang and LLVM being used are compatible. Refer to [the documentation](https://llvm.org/docs/CMake.html "Building LLVM with CMake") for any help on building LLVM from source.

* Create a new LLVM pass, copy the code given in [src directory](./src "src directory"). Refer to [the documentation](http://llvm.org/docs/WritingAnLLVMPass.html "Writing an LLVM Pass") for any help on writing or running an LLVM pass.

## Interpreting the output

* First the translated LLVM code corresponding to the program is given.

* This is followed by the control flow graph corresponding to the program.
  * The nodes corresponds to reachable instructions and confluence points, alongwith two additional special nodes - START and END.
  * Each node in the control flow graph is also assigned a CFG index which is mentioned first.
  * This is followed by the node type followed by information about it depending on its type.
    * For transfer points - the basic block corresponding to the instruction, the instruction itself and the predecessor node index.
    * For confluence points - the list of indexes of predecessor nodes along with their basic block of the instruction corresponding to nodes.
    * For START node - START is mentioned.
    * For END node - the list of indexes of predecessor nodes.

* After this the Herbrand Equivalence analysis information is given.
  * First the initial partition (at the START point) is given, followed by partitions at each program point for each iteration.
  * The information at a program point for an iteration contains the equivalence classes along with the set identifiers assigned to the sets.

**NOTE** - The variable names in the output will not be same as those in the input C/C++ source files. Refer to LLVM code at the beginning of the output for resolving variable names.

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

## Directory structure

* **documentation** - Folder containing [Doxygen](http://www.doxygen.nl/ "Doxygen") documentation files, generated from inline comments

* **src** - Folder containing the LLVM pass - implementation of the algorithm  

* **testcases** - Folder containing testcases used for verification of the algorithm

* **Doxyfile** - File containing doxygen configurations used for generating the documentations

* **README.md** - Readme file

## References

* [A fix-point characterization of Herbrand equivalence of expressions in data flow frameworks](https://arxiv.org/abs/1708.04976 "A fix-point characterization of Herbrand equivalence of expressions in data flow frameworks")
* [Clang](https://clang.llvm.org/ "Clang")
* [LLVM](https://llvm.org/ "LLVM Compiler Infrastructure")
* [Building LLVM with CMake](https://llvm.org/docs/CMake.html "Building LLVM with CMake")
* [Writing an LLVM Pass](http://llvm.org/docs/WritingAnLLVMPass.html "Writing an LLVM Pass")
