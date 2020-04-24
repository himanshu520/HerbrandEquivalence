
# Herbrand Equivalence Analysis

Implementation of Herbrand Equivalence analysis algorithm mentioned [here](https://arxiv.org/abs/1708.04976 "A fix-point characterization of Herbrand equivalence of expressions in data flow frameworks") for a toy language.

## Toy Language

There are three different kinds of statements in any program of the toy language.

* **Normal Instructions**
  * These are of one of the following three form where `x` should be a variable, `y` and `z` can be either an integer or a variable. The last instruction specifies a **non-deterministic assignment**.
    * `x = y`
    * `x = y + z`
    * `x = *`
  * Variables need not be declared before they are used.

* **`LABEL` statements**
  * Such statements defines a label which acts as an alias for the immediate next statement of the *first kind* (ie. the normal instructions). A `LABEL` statement occurring at the end refers to the *end of the program*.
  * It has the following format  
    `LABEL identifier1 identifier2 ...`
  * A statement can either have a single label or multiple labels. Multiple labels can either be defined in the same `LABEL` statement or different statements.
  * All the `LABEL` identifiers used in the definitions must be unique (even those used for the same instruction).

* **`GOTO` statements**
  * `GOTO` statements are used to specify jumps - the successors of immediately preceding instruction of the *first kind* (ie. the normal instructions). A `GOTO` statement at the beginning means two different control flow paths from the *start of the program*.
  * A `GOTO` statement has the following format.  
    `GOTO identifier1 identifier2 ...`  
  * Again like `LABEL` statements, for specifying multiple successors of an instruction there can either be a single `GOTO` statement with multiple label identifiers or multiple `GOTO` statements.
  * For Herbrand Equivalence analysis the condition of the jumps are irrelavent, so in the *toy language* all the jumps are unconditional.
  * For instructions of the *first kind* without `GOTO` its successor, by default, is the immediate occurring instruction of the *first kind*. However, if such instructions has a corresponding `GOTO`, all the successors *must* be specified there itself - they do not have any default successors.
  * The labels used in the `GOTO` statement must be defined in some `LABEL` statement in the program.

#### Other important points

* Each instruction must be specified a single line and should be the only instruction in that line.
* Empty lines in the program text are ignored.
* A normal instruction can contain operators other than `+`.
* A variable name must start with an alphabetic character.
* Specify the program properly with atleast one whitespace between the tokens. Check whether the program was parsed properly by looking at the output after the program is run.
* Variables and constants defined in any unreachable instruction are also considered for analysis, but the instruction itself is ignored.

## Directory structure

* **documentation** - Folder containing [Doxygen](http://www.doxygen.nl/ "Doxygen") documentation files, generated from inline comments.

* **src** - Folder containing the LLVM pass - implementation of the algorithm.
  * **HerbrandEquivalence.cpp** - This file contains actual implementation of the algorithm for Herbrand Equivalence analysis.
  * **MapVector.h** - This file defines a MapVector class that combines the powers of std::map and std::vector for forward mapping objects of arbitrary types to integers and also reverse mapping.
  * **Program.h** - This file defines classes to capture a program and also provides functionalities for working with it.

* **testcases** - Folder containing testcases used for verification of the algorithm.

* **Doxyfile** - File containing doxygen configurations used for generating the documentations.

* **README.md** - Readme file.

## How to run

* The source files are in the *src* directory. Compile the files as `g++ HerbrandEquivalence.cpp -o HerbrandEquivalence`.

* Run the executable for a file *sourceFile* containing a toy program as `./HerbrandEquivalence sourceFile`.

## Interpreting the output

* First a list of constants and variables in the program listed.

* Then the parsed program itself is printed with information of each instruction in a different line.
  * Each instruction in the input program is assigned an instruction index which is given followed by the instruction itself.
  * Also, for each instruction the indexes of predecessor instructions is given.
  * There are also two additional dummy instructions - START and END.

* This is followed by the control flow graph corresponding to the program.
  * The nodes corresponds to reachable instructions and confluence points, alongwith two additional special nodes - START and END.
  * Each node in the control flow graph is also assigned a CFG index which is mentioned first.
  * This is followed by the node type followed by information about it depending on its type.
    * For transfer points - the instruction index, the instruction itself and the predecessor node index.
    * For confluence points - the list of indexes of predecessor nodes.
    * For START node - START is mentioned.
    * For END node - the list of indexes of predecessor nodes.

* After this the Herbrand Equivalence analysis information is given.
  * First the initial partition (at the START point) is given, followed by partitions at each program point for each iteration.
  * The information at a program point for an iteration contains the equivalence classes along with the set identifiers assigned to the sets.

## References

* [A fix-point characterization of Herbrand equivalence of expressions in data flow frameworks](https://arxiv.org/abs/1708.04976 "A fix-point characterization of Herbrand equivalence of expressions in data flow frameworks")
