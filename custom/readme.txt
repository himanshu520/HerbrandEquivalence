An iterator is any object that, pointing to some element in a range of elements (such as an array or a container), has the ability to iterate through the elements of that range using a set of operators (with at least the increment (++) and dereference (*) operators).

There are five kinds of iterators for a container - input <> output < forward (input + output) < bidirectional < random-access iterators.

Input iterators
    - They are the simplest of the iterators and can be used in sequential input operations, where each value pointed by the iterator is read only once and then the iterator is incremented
    - Are single pass only (over the range) - doesn't provide functionality for multiple pass
    - Allows equality and inequality comparison - but not other relational operators
    - Allows dereferencing for use as Rvalue - but can't modify the value (can't be used as Lvalue)
    - Are incrementable (++X or X++) - but can't be decremented (that is why only single pass) and also no arithematic operations
    - The values pointed to be these iterators can be swapped

Output iterators
    - Same as input iterators, but instead of accessing value - are used to modify values (used as Lvalue but not Rvalue)

Forward iterators
    - Combines the power of both input and output iterators - can be used as both Lvalue and Rvalue

Bidirectional iterators
    - Same as forward iterators but also provides are also decrementable and hence can be used in multipass algorithms

Random-access iterators
    - Same as bidirectional operators but also provides functionality of applying arithematic and relational operators
    - So, we can jump from one location to other without visiting the intermediate locations (that is why it is random access)
Forward iterators allows values to be both accessed and modified.










To solve circular dependency, use forward declaration and 
https://stackoverflow.com/questions/1748624/circular-dependencies-of-declarations?rq=1
https://stackoverflow.com/questions/553682/when-can-i-use-a-forward-declaration