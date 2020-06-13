/** 
 * @file HerbrandEquivalence.cpp
 *  This file defines implements algorithm for Herbrand
 *  Equivalence analysis.
 **/

#include"Program.h"

// simple macro to print a header line to standard output
#ifndef PRINT_HEADER
#define PRINT_HEADER(str) std::cout << std::string(100, '=') << '\n' << (str) \
                                    << '\n' << std::string(100, '=') << '\n';
#endif

#define VAR_VAL(x) {false, x}
#define NULL_VAL {false, -1}
#define CONST_VAL(x) {true, x}

/**
 * @brief Generic function to find the intersection of two std::set.
 * 
 * @tparam          T     Any valid type for which std::set can be created.
 * @param[in]       xset  Reference to first input set.
 * @param[in, out]  yset  Reference to second input set.
 * @return          Void
 * 
 * @note The first input argument is modified to contain the intersection.
 **/
template <typename T>
void setIntersect(std::set<T> &xset, std::set<T> &yset) {
    for(auto it = xset.begin(); it != xset.end(); ) {
        auto it_ = it++;
        if(yset.find(*it_) == yset.end())
            xset.erase(it_);
    }
}

/**
 * @brief
 *  Vector of operators used in the program.
 * 
 * @note
 *  This vector needs to be updated by adding operators
 *  if the programs on which it is being run contains
 *  more operators.
 **/
std::vector<char> Ops({'+'});

/**
 * @brief
 *  `std::map` to map expressions to integer indexes. This
 *  indexing is arbitrarily fixed in the beginning and
 *  used throughout wherever indexing is required for the
 *  expressions.
 * 
 * @see     Partitions
 **/
std::map<Program::ExpressionTy, int> IndexExp;

/**
 * @brief
 *  Counter to keep track of set identifiers. New set 
 *  identifiers are created by incrementing this counter.
 * 
 * @see     Partitions, Program::ExpressionTy
 **/
int SetCnt = 0;

/**
 * @brief
 *  Vector to keep track of equivalence classes at each
 *  program point.
 * 
 * @details
 *  There is an entry for each program point in this vector
 *  which itself is a vector containing an entry for each 
 *  expression of length atmost two. The entries for the 
 *  expressions contains integer set identifiers. For a
 *  given program point two expressions are equivalent iff
 *  they have the same set identifier.
 * 
 * @see     IndexExp, SetCnt
 **/
std::vector<std::vector<int>> Partitions;

/**
 * @brief
 *  `std::map` to hold parent set identifiers.
 * 
 * @details
 *  a1 =~ a2 and b1 =~ b2 at a program point iff
 *  (a1 + b1) =~ (a2 + b2). Parent map helps us 
 *  to resolve this by storing set identifiers of 
 *  compound expression (a1 + b1) as parent of set 
 *  identifiers of its smaller sub-expressions (a1 
 *  and b1). Now, later when a new expression is 
 *  formed by combining expressions (a2 and b2) with 
 *  same set identifiers as those of the two 
 *  sub-expressions (a1 and b1), we assign it 
 *  (a2 + b2) the same identifier as the previous 
 *  compound expression (a1 + b1).
 *  This map also helps to resolve other complex 
 *  cases where some equivalence classes are lost in 
 *  the middle of the program but appears later.
 *  
 * 
 * @note
 *  The two integers in the key of this map are ordered.
 *  Also this map is global used throughout the program.
 * 
 * @see     IndexExp, Partitions, SetCnt
 **/
std::map<std::tuple<char, int, int>, int> Parent;

/**
 * @brief Captures a program text.
 * 
 * @see     Program
 **/
Program program;

/**
 * @brief 
 *  Maps expressions of length atmost two to integers
 *  arbitrarily for indexing purpose by updating 
 *  `IndexExp` map.
 * 
 * @return  Void
 * 
 * @see     IndexExp, Program::ExpressionTy
 **/
void assignIndex() {
    // vector storing constants and variables in the program
    std::vector<Program::ValueTy> CuV;

    // indexing of expressions starts from 0
    int expIdx = 0;

    // mapping constants    
    for(int i = 0; i < program.Constants.size(); i++) {
        CuV.push_back({true, i});
        IndexExp[{'\0', CONST_VAL(i), NULL_VAL}] = expIdx++;
    }
    
    // mapping variables
    for(int i = 0; i < program.Variables.size(); i++) {
        CuV.push_back({false, i});
        IndexExp[{'\0', VAR_VAL(i), NULL_VAL}] = expIdx++;
    }

    // mapping length two expressions
    for(auto op : Ops)
        for(auto &leftVal : CuV)
            for(auto &rightVal : CuV)
                IndexExp[{op, leftVal, rightVal}] = expIdx++;
}

/**
 * @brief Checks whether two partitions are same.
 * 
 * @details
 *  Two partitions are same if in the vectors representing
 *  them values at two indexes are equal in the first 
 *  vector iff they are equal in the second vector.
 * 
 * @param[in]   first   First partition
 * @param[in]   second  Second partition
 * @return      Returns  true if the two partitions are same,
 *              otherwise false
 * 
 * @see     IndexExp, Partitions
 **/
bool samePartition(std::vector<int> const &first, std::vector<int> const &second) {
    // map to store equivalent indexes (have same value) 
    // in the first partition
    std::map<int, std::vector<int>> mp;
    for(int i = 0; i < (int)first.size(); i++)
        mp[first[i]].push_back(i);

    // checking if equivalent indexes in the first 
    // partition are also equivalent in the second.
    // If not `false` is returned
    for(auto &el : mp) {
        int elSetId = second[el.second[0]];
        for(auto &nel : el.second)
            if(second[nel] != elSetId) return false;
    }

    // the partitions are equivalent, so return `true`
    return true;
}

/**
 * @brief 
 *  return  set identifier for a given two length expression 
 *  at a program point.
 * 
 * @details
 *  With the help of `Parent` map, first a check is made if 
 *  a set identifier representing the expression at the
 *  current program point exists. If so it is returned else
 *  a new set identifier is created by updating `SetCnt` which
 *  is returned. This information is also stored in the 
 *  `Parent` map for future use.
 * 
 * @param[in]   partition   Vector representing the partition 
 *                          at the program point.
 * @param[in]   exp         Expression whose set identifier is
 *                          required at the program point.
 * @return      The set identifier for the expression at the 
 *              program point.
 * 
 * @note    Make sure that the second argument passed represents a 
 *          length two expression.
 * 
 * @see     IndexExp, Parent, Partitions, SetCnt
 **/
int findSet(std::vector<int> const &partition, Program::ExpressionTy const &exp) {
    // set identifier corresponding to left and right 
    // subexpressions at the current program point
    int leftSetId = partition[IndexExp[{'\0', exp.leftOp, NULL_VAL}]];
    int rightSetId = partition[IndexExp[{'\0', exp.rightOp, NULL_VAL}]];

    // checking if a set representing the expression
    // already exists
    std::tuple<char, int, int> tup = {exp.op, leftSetId, rightSetId};
    auto it = Parent.find(tup);

    // if the set already exists return its identifier,
    // otherwise return new set identifier and update
    // `Parent` map with this information
    if(it == Parent.end())
        return Parent[tup] = SetCnt++;
    return it->second;
}

/**
 * @brief Finds initial partition.
 * 
 * @details
 *  Returns a vector representing initial partition
 *  in which all expressions are in different 
 *  partition (has non-equivalent set identifiers).
 *  It also updates `Parent` map for identifiers
 *  assigned to length two expressions.
 * 
 * @param[out]  partition   Vector to store the 
 *                          initial partition.
 * @return      Void
 * 
 * @see findSet, IndexExp, Partitions
 **/
void findInitialPartition(std::vector<int> &partition) {
    // initialise partition
    partition.assign(IndexExp.size(), -1);

    // create new IDstruct object for each epxression.
    // For length two expressions this is done indirectly
    // by calling `findSet` function which also updates
    // `Parent` map
    for(auto &el : IndexExp) {
        if(el.first.op == '\0') partition[el.second] = SetCnt++;
        else partition[el.second] = findSet(partition, el.first);
    }
}

/**
 * @brief
 *  Returns equivalence class of an expression at a 
 *  given program point.
 * 
 * @details
 *  The equivalence class is the set of indexes corresponding
 *  to the expressions (as given by `IndexExp`) which have 
 *  same set identifiers as the index representing the expression 
 *  (passed as parameter).
 *  
 *
 * @param[in]   partition   Vector representing the partition
 *                          at current program point.
 * @param[in]   expIdx      Index of expression whose equivalence
 *                          class is required.
 * @param[out]  expClass    Set containing identifiers corresponding
 *                          to the required equivalence class.
 * @return      Void
 * 
 * @see IndexExp, Partitions
 **/
void getClass(std::vector<int> const &partition, int expIdx, std::set<int> &expClass) {
    expClass.clear();
    int expSetId = partition[expIdx];

    for(int i = 0; i < IndexExp.size(); i++) {
        if(expSetId == partition[i])
            expClass.insert(i);
    }
}

/**
 * @brief Prints a partition in readable format.
 * 
 * @param[in]   partition   The partition vector to 
 *                          be printed.
 * @return      Void
 * 
 * @see IndexExp, Partitions
 **/
void printPartition(std::vector<int> const &partition) {
    // if any index stores -1, then the whole vector
    // stores -1, representing the TOP element
    if(partition[0] == -1) {
        std::cout << "<TOP ELEMENT>";
        return;    
    }

    // finding equivalent expressions in `mp` map
    std::map<int, std::vector<Program::ExpressionTy>> mp;
    for(auto &el : IndexExp)
        mp[partition[el.second]].push_back(el.first);

    // print the equivalence classes along with their
    // set identifiers
    for(auto &el : mp) {
        std::cout << '[' << el.first << "]{";

        int sz = el.second.size();
        for(int i = 0; i < sz; i++) {
            program.cout << el.second[i]; 
            if(i != sz - 1) program.cout << ", ";
        }

        std::cout << "}, ";
    }
}

/**
 * @brief Transfer function associated with Herbrand analysis.
 * 
 * @param[in]   cfgIndex    Control flow graph node index on 
 *                          which transfer function is applied.
 *                          The function modifies 
 *                          `Partitions[cfgIndex]`.
 * @returns     Void
 * 
 * @see findSet, IndexExp, Partitions, Program
 **/
void transferFunction(int cfgIndex) {
    // current partition vector
    std::vector<int> &partition = Partitions[cfgIndex];

    // first copy predecessor partition into current partition
    partition = Partitions[program.CFG[cfgIndex].predecessors[0]];

    // if the current partition has any index with value -1, it 
    // means that it represents the TOP element and it has to be
    // left as such without any modifications
    if(partition[0] == -1) return;

    // if the node is the END CFG node, return
    int instIdx = program.CFG[cfgIndex].instructionIndex;
    if(instIdx == program.Instructions.size() - 1) return;

    // `changedExp` is expression (ie. a constant or a variable) 
    // which has been assigned value and `changedToExp` is the 
    // expression which has been assigned to it
    Program::InstructionTy &inst = program.Instructions[instIdx];
    Program::ExpressionTy changedExp = {'\0', inst.lValue, NULL_VAL};
    Program::ExpressionTy changedToExp = inst.rValue;

    if(changedToExp.op == '#') {
        // if it is a non-deterministic assignment, then create a
        // new set identifier
        partition[IndexExp[changedExp]] = SetCnt++;
    } else {
        // assign the `changedExp`, the set identifier of 
        // `changedToExp`
        partition[IndexExp[changedExp]] = partition[IndexExp[changedToExp]];
    }

    // update set identifiers for two length expressions 
    // involving `changedExp`. Here set identifiers of all 
    // two length expressions are assigned values, but only
    // those involving `changedExp` are updated
    for(auto &el : IndexExp) {
        if(el.first.op == '\0') continue;
        partition[el.second] = findSet(partition, el.first);
    }
}

/**
 * @brief Confluence function associated with Herbrand analysis.
 * 
 * @param[in]   cfgIndex    Control flow graph node index on 
 *                          which confluence function is applied.
 * @returns     Void
 * 
 * @see findSet, IndexExp, Partitions, Program
 **/
void confluenceFunction(int cfgIndex) {
    // vector of  predecessor CFG node indexes
    std::vector<int> &predecessors = program.CFG[cfgIndex].predecessors;
    
    // if all the predecessors partition represents TOP 
    // element then their confluence is also TOP element.
    // So, the current partition need not be modified as
    // by initialisation the current partition is also the
    // TOP element
    bool cont = false;
    for(auto pred : predecessors)
        cont |= (Partitions[pred][0] != -1);
    if(not cont) return;

    // the current partition
    std::vector<int> &partition = Partitions[cfgIndex];

    // to check which expressions has already been processed
    std::vector<bool> accessFlag(IndexExp.size(), false);

    // process all the expressions one by one
    for(auto &el : IndexExp) {
        int elIdx = el.second;

        // continue if the expression is already processed
        if(accessFlag[elIdx]) continue;

        // mark it as processed
        accessFlag[elIdx] = true;

        // if the set identifier for the expression in all
        // the predecessors is same it is assigned the same
        // set identifier; else a new set identifier is 
        // created and assigned to all the expressions which
        // belongs to the same equivalence class as the 
        // current expression across all the predecessors and
        // also mark them as processed
        int elSetId = -1;
        bool flag = true;
        for(auto pred : predecessors) {
            int elSetIdPred = Partitions[pred][elIdx];
            if(elSetIdPred == -1) continue;
            else if(elSetId == -1) elSetId = elSetIdPred;
            else if(elSetId != elSetIdPred) flag = false;
        }

        if(flag) partition[elIdx] = elSetId;
        else {
            std::set<int> intersection, elClassPred;
            for(int i = 0; i < IndexExp.size(); i++)
                intersection.insert(i);
            
            for(auto pred : predecessors) {
                getClass(Partitions[pred], elIdx, elClassPred);
                setIntersect<int>(intersection, elClassPred);
            }

            int newSetId = SetCnt++;
            for(auto nel : intersection)
                accessFlag[nel] = true, partition[nel] = newSetId;
        }
    }

    // now update `Parent` map
    for(auto &el : IndexExp) {
        if(el.first.op == '\0') continue;
        
        int leftSetId = partition[IndexExp[{'\0', el.first.leftOp, NULL_VAL}]];
        int rightSetId = partition[IndexExp[{'\0', el.first.rightOp, NULL_VAL}]];

        Parent[{el.first.op, leftSetId, rightSetId}] = partition[el.second];
    }
}

/**
 * @brief Main Herbrand analysis function.
 * 
 * @returns     Void
 * 
 * @see Partitions, IndexExp, Parent, Program
 **/
void HerbrandEquivalence() {
    PRINT_HEADER("Herbrand Equivalence Computation");
    std::cout << "\n";

    // assign index to expressions arbitrarily
    assignIndex();

    // initialise partition vector with -1 for each program
    // points and each expression - this stands for TOP 
    // partition at each program point. Note that any element
    // of partition vector being -1 means that whole vector
    // holds -1 and represents TOP partition
    Partitions.assign(program.CFG.size(), 
                      std::vector<int>(IndexExp.size(), -1));

    // initialise starting partition for START node
    findInitialPartition(Partitions[0]);

    PRINT_HEADER("Initial Partition");
    printPartition(Partitions[0]);
    std::cout << "\n\n\n";

    bool converged = false;
    int iterationCtr = 0;

    // repeat while convergence
    while(not converged) {
        PRINT_HEADER("Iteration " + std::to_string(++iterationCtr));
        converged = true;

        // for all program points (nodes in CFG) except
        // START apply transfer/confluence function as
        // applicable
        for(int i = 1; i < program.CFG.size(); i++) {
            std::vector<int> oldPartition = Partitions[i];
            std::vector<int> &predecessors = program.CFG[i].predecessors;
            int instIdx = program.CFG[i].instructionIndex;

            std::cout << '[' << i << "] : ";
            if(predecessors.size() > 1) {
                // if CFG node corresponds to a confluence point

                std::cout << "Confluence of [ ";
                for(auto el : predecessors)
                    std::cout << el << " ";
                std::cout << "]\n\t";

                confluenceFunction(i);
            } else {
                // if CFG node corresponds to a transfer point

                if(instIdx != program.Instructions.size() - 1) {
                    program.cout << "Transfer Point => (" << instIdx << ") " 
                                 << program.Instructions[instIdx];
                } else program.cout << "END";

                program.cout << " [" << predecessors[0] << "]\n\t";

                transferFunction(i);
            }

            printPartition(Partitions[i]);
            std::cout << "\n\n";

            // update convergence flag
            if(not samePartition(oldPartition, Partitions[i]))
                converged = false;
        }
        std::cout << "\n\n";
    }
}

int main(int argc, char **argv) {
    // parse and print the program
    program.parse(std::string(argv[1])), program.print();

    // create and print the control flow graph
    program.createCFG(), program.printCFG();

    // perform Herbrand equivalence analysis
    HerbrandEquivalence();

    return 0;
}