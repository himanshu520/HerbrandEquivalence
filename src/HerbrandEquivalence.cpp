#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Instruction.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/Support/Debug.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include <bits/stdc++.h>

using namespace llvm;
using namespace std;

#define PRINT(str) errs() << string(100, '=') << "\n" << (str) \
                          << "\n" << string(100, '=') << "\n"
#define DEBUG 1

/**
 * @brief Generic function to find the intersection of two std::set.
 * 
 * @tparam          T     Any valid type for which std::set can be created
 * @param[in]       xset  Reference to first input set
 * @param[in, out]  yset  Reference to second input set
 * @return          Void
 * 
 * @note        The first input argument is modified to contain the intersection.
 **/
template <typename T>
void setIntersect(set<T> &xset, set<T> &yset) {
    for(auto it = xset.begin(); it != xset.end(); ) {
        auto it_ = it++;
        if(yset.find(*it_) == yset.end())
            xset.erase(it_);
    }
}

/**
 * @brief Generic function to find the union of two std::set.
 * 
 * @tparam          T     Any valid type for which std::set can be created
 * @param[in]       xset  Reference to first input set
 * @param[in, out]  yset  Reference to second input set
 * @return          Void
 * 
 * @note        The first input argument is modified to contain the union.
 **/ 
template <typename T>
void setUnion(set<T> &xset, set<T> &yset) {
    for(auto el : yset)
        xset.insert(el);
}

/** 
 * @namespace
 * Namespace containing the LLVM pass 
 **/
namespace HerbrandPass {

    /** 
     * @struct IDstruct
     * @brief Structure for capturing Herbrand Equivalence classes.
     * 
     * @details 
     *  At a given program point, expressions pointing to the same 
     *  IDstruct object are Herbrand Equivalent at that point.
     * 
     * @note
     *  Expressions across different program points, pointing to
     *  the same IDstruct object also have equivalent value and this
     *  information can be used for global value numbering.
    **/
    struct IDstruct {
        /**
         * @brief 
         *  Operator of the Herbrand class represented by the object.
         * 
         * @details
         *  For a constant or variable it is just null character. 
         *  For compound expressions, it represents the operator 
         *  which combines the left and right subexpressions.
         **/
        char opSymbol;

        /**
         * @brief Count of number of pointers to the object.
         * 
         * @details
         *  Whenever a variable points to an IDstruct object,
         *  parentCnt of the object is incremented. Similarly, 
         *  when a variable stops pointing to the object,
         *  parentCnt is decremented. If at any point of time
         *  parentCnt is 0, then it means that the object 
         *  is of no use and can be destroyed to free memory.
         **/
        int parentCnt;

        /**
         * @brief 
         *  Pointer to IDstruct object corresponding to the equivalence
         *  class of left subexpression of the current object.
         **/
        IDstruct *leftID;
        
        /**
         * @brief 
         *  Pointer to IDstruct object corresponding to the equivalence
         *  class of right subexpression of the current object.
         **/
        IDstruct *rightID;

        /**
         * @brief Default constructor for the structure.
         **/
        IDstruct() : opSymbol('\0'), parentCnt(0), leftID(nullptr), rightID(nullptr) {}

        /**
         * @brief Parameterised constructor for the structure.
         * 
         * @param   op      Operator for the current Herbrand Equivalence class
         * @param   leftID  Pointer to the class corresponding to left subexpression
         * @param   rightID Pointer to the class corresponding to right subexpression
         **/
        IDstruct(char op, IDstruct *leftID, IDstruct *rightID) {
            this->opSymbol = op;
            this->parentCnt = 0;
            this->leftID = leftID;
            this->rightID = rightID;
        }
    };

    /** 
     * @brief Represents a two operand expression in prefix form.
     * 
     * @details 
     *  In LLVM expressions are of type Value*. An expTuple
     *  of the form {op, left, right} represents an expression
     *  "left op right".
     **/
    typedef tuple<char, Value *, Value *> expTuple;

    /**
     * @brief
     *  Vector to hold equivalence information for each
     *  constant/variable/two operand expression at a
     *  program point.
     **/
    typedef vector<IDstruct *> Partition;

    /** 
     * @brief Set of constants used in the program.
     **/
    set<Value *> Constants;
    
    /** 
     * @brief Set of variables used in the program.
     **/
    set<Value *> Variables;
    
    /**
     * @brief Set of constants and variables used in the program.
     **/
    set<Value *> CuV;

    /**
     * @brief Set of operators used in the program.
     **/
    set<char> Ops({'+'});

    /**
     * @brief 
     *  Maps constants and variables in the program to integers
     *  for indexing purpose.
     **/
    map<Value *, int> indexCV;

    /**
     * @brief 
     *  Maps all possible two operand expressions that can be formed
     *  using constants, variables and operators in the program to
     *  integers for indexing purpose.
     **/
    map<expTuple, int> indexExp;

    /**
     * @brief 
     *  Count of number of expressions of length atmost two that
     *  can be formed using constants and variables in the program.
     **/
    int numExps;

    /**
     * @brief 
     *  Stores predecessors for each instruction - an instruction
     *  can have zero, one or more than one predecessors.
     **/
    map<Instruction *, vector<Instruction *>> Predecessors;

    /**
     * @brief 
     *  Stores the instructions which represents confluence points - an
     * instruction having more than one predecessors.
     **/
    set<Instruction *> confluencePoints; 

    /**
     * @brief
     *  Stores the variables available at each point in the program.
     * 
     * @details
     *  Calculation of \c availVariables is a forward dataflow problem, 
     *  calculated via fixed point computation. A variable is available 
     *  at a point if all paths from the start of the program to that 
     *  point contains a definition of the variable. Our universe is
     *  <b> U - the set of program variables </b> and for each program 
     *  point <b> P </b>, we initialise <b> OUT[P] </b> to 
     *  <b> U </b>. Also, let <b> GEN[P] </b> to be the variable 
     *  defined at <b> P </b> if any, otherwise empty set. We will perform 
     *  updates as \n \n
     *  <tt> IN[P] = intersect(availVariables[P']) </tt>
     *  <tt> OUT[P] = union(IN[P], GEN[P]) </tt>
     *  \n \n where \c union is normal set union and \c intersect is 
     *  performed over all <b> P' </b> belonging to the predecessor of <b> P </b>.
     *  Finally, <tt> availVariables[P] = OUT[P] </tt>.
     * 
     * @note
     *  This is different from both available expressions and reaching
     *  definition.
     * 
     * @see Reaching definition, available expression
     *  
     **/
    map<Instruction *, set<Value *>> availVariables;

    /**
     * @brief 
     *  Stores Herbrand Equivalence information after each instruction
     *  in the program.
     * 
     * @details
     *  Each index of Partition, corresponds to either a 
     *  constant or a variable or a two operand expression as determined
     *  by indexCV and indexExp. Those variables/constants/expressions that
     *  point to the same IDstruct object (has same pointer value) at a
     *  given program point are Herbrand Equivalent at that program point.
     * 
     * @see indexCV, indexExp, Partition
     **/
    map<Instruction *, Partition> partitions;

    /**
     * @brief 
     *  For a given {op, leftID, rightID} as key, stores pointer to
     *  IDstruct object whose fields are {op, _, leftID, rightID}.
     * 
     * @details
     *  When we create an IDstruct object with fields 
     *  {op, _, leftID, rightID}, we store this information in the
     *  map so that when we again need such an object we do not
     *  create it again and use the already existing one.
     **/
    map<tuple<char, IDstruct *, IDstruct *>, IDstruct *> Parent;

    /**
     * @brief 
     *  Returns operator symbol corresponding to the 
     *  output of llvm::Instruction::getOpcodeName function.
     * 
     * @param[in]   opCodeName  A string returned by getOpcodeName
     * @returns     The character symbol corresponding to the operator name
     * 
     * @see llvm::Instruction::getOpcodeName
     **/
    char getOpSymbol(string opCodeName) {
        if(opCodeName == "add") return '+';
        if(opCodeName == "sub") return '-';
        if(opCodeName == "mul") return '*';
        if(opCodeName == "sdiv") return '/';
        if(opCodeName == "udiv") return '/';
        return '\0';
    }

    /**
     * @brief 
     *  Initialises Constants, Variables, CuV, indexExp and indexCV
     *  by looking through the instructions in the program.
     * 
     * @param[in]   F     Function block over which we are operating
     * @returns     Void
     * 
     * @see Constants, CuV, indexCV, indexExp, Variables
     **/
    void assignIndex(Function &F) {
        // first find the set of constants and variables
        // by iterating over the instructions
        for(Instruction &I : instructions(&F)) {
            // if the instruction is not of void type then 
            // it represents a variable. All the variables
            // will be covered in this case
            if(!I.getType()->isVoidTy()) 
                Variables.insert(&I);

            // now we iterate over its operands to find the 
            // constants, this case won't add any extra variables
            // Also, we should skip alloca instruction as its
            // operands doesn't contain any constant that
            // concerns us
            if(isa<AllocaInst>(&I)) continue;
            for(int i = 0; i < (int)I.getNumOperands(); i++) {
                Value *value = I.getOperand(i);
                if(dyn_cast<ConstantInt>(value)) {
                    Constants.insert(value);
                }
            }
        }
        CuV = Constants, CuV.insert(Variables.begin(), Variables.end());

        // now assign integer indexes to variables, constants
        // and two operand instructions
        int ctr = 0;
        for(auto el : CuV) indexCV[el] = ctr++;
        for(auto op : Ops)
            for(auto left : CuV)
                for(auto right : CuV)
                    indexExp[{op, left, right}] = ctr++;

        // update the value of numExps
        numExps = ctr;
    }

    /**
     * @brief 
     *  Finds initial partition in which every expression is in a separate
     *  equivalence class.
     * 
     * @param[out]  v    Vector to hold the initial partition
     * @returns     Void
     * 
     * @see Partition
     **/
    void findInitialPartition(Partition &partition) {
        // initialise partition
        partition.assign(numExps, nullptr);

        // create new IDstruct object for each constant and variable
        for(auto el : CuV) {
            IDstruct *ptr = new IDstruct;
            partition[indexCV[el]] = ptr;
            ptr->parentCnt += 1;
        }

        // create new IDstruct object for each possible two operand
        // expression
        for(auto op : Ops)
            for(auto left : CuV)
                for(auto right : CuV) {
                    IDstruct *leftID = partition[indexCV[left]];
                    IDstruct *rightID = partition[indexCV[right]];

                    IDstruct *ptr = new IDstruct(op, leftID, rightID);
                    Parent[{op, leftID, rightID}] = ptr;
                    // increment parentCnt of leftID and rightID, because
                    // the new object starts pointing at them
                    leftID->parentCnt += 1;
                    rightID->parentCnt += 1;

                    partition[indexExp[{op, left, right}]] = ptr;
                    // increment parentCnt of the new object because the
                    // current expression in the partition points to it
                    ptr->parentCnt += 1;
                }
    }

    /**
     * @brief 
     *  Finds the equivalence class corresponding to a constant, 
     *  variable or two operand expression at a given program point.
     * 
     * @param[in]   index       Index corresponding to the 
     *                          variable/constant/expression as
     *                          given by indexCV/indexExp
     * @param[in]   partition   The Equivalence information at
     *                          the program point
     * @param[out]  setCV       Set of variables/constants belonging
     *                          to the equivalence class
     * @param[out]  setExp      Set of two operand expressions belonging
     *                          to the equivalence class
     * @returns     Void
     * 
     * @see indexCV, indexExp, Partition, partitions
     **/
    void getClass(int index, Partition const &partition,
                  set<Value *> &setCV, set<expTuple> &setExp) {

        IDstruct *ptr = partition[index];
        setCV.clear(), setExp.clear();
        
        if(ptr == nullptr) return;

        // compare the values in partition vector with ptr,
        // if they are same then the corresponding expression
        // is in the required equivalence class
        for(auto &el : indexCV)
            if(partition[el.second] == ptr)
                setCV.insert(el.first);
        for(auto &el : indexExp)
            if(partition[el.second] == ptr)
                setExp.insert(el.first);
    }

    /**
     * @brief 
     *  Checks whether two partitions are equivalent or not.
     * 
     * @note
     *  We don't need to check whether two expressions at
     *  different points are equivalent. Instead, we are 
     *  checking if the equivalence class of each expression
     *  at two different program points are same.
     * 
     * @param[in]   first       Equivalence information for the
     *                          first partition
     * @param[in]   second      Equivalence information for the
     *                          second partition
     * @returns     true/false
     * 
     * @see Partition, partitions
     **/
    bool samePartition(Partition const &first, Partition const &second) {

        // variables to store the equivalence class for
        // first and second partitions
        set<Value *> setCVfirst, setCVsecond;
        set<expTuple> setExpfirst, setExpsecond;

        // to check which expressions have already been considered
        vector<bool> done(numExps, false);

        for(auto el : indexCV) {
            if(done[el.second]) continue;

            getClass(el.second, first, setCVfirst, setExpfirst);
            getClass(el.second, second, setCVsecond, setExpsecond);
            if(setCVfirst != setCVsecond or setExpfirst != setExpsecond)
                return false;

            // mark other constants/variables/expressions in the equivalence
            // class as done
            for(auto el_ : setCVfirst) done[indexCV[el_]] = true;
            for(auto el_ : setExpfirst) done[indexExp[el_]] = true;
        }

        for(auto el : indexExp) {
            if(done[el.second]) continue;

            getClass(el.second, first, setCVfirst, setExpfirst);
            getClass(el.second, second, setCVsecond, setExpsecond);
            if(setCVfirst != setCVsecond or setExpfirst != setExpsecond)
                return false;

            // mark other constants/variables/expressions in the equivalence
            // class as done
            for(auto el_ : setCVfirst) done[indexCV[el_]] = true;
            for(auto el_ : setExpfirst) done[indexExp[el_]] = true;
        }
        return true;
    }

    /**
     * @brief Function to decrement parentCnt of an IDstruct object.
     * 
     * @details
     *  If parentCnt of an IDstruct object becomes zero then it 
     *  means that the object is no longer of any use and its memory
     *  can be freed. In the second case, we need to recursively call
     *  decreaseParentCnt for its left and right.
     * 
     * @param[in, out]  ptr  Pointer to the IDstruct object
     * @returns         Void
     * 
     * @see IDstruct
     **/
    void decreaseParentCnt(IDstruct *&ptr) {
        if(ptr == nullptr) return;

        ptr->parentCnt -= 1;
        if(ptr->parentCnt > 0) return;

        // object pointed by ptr has no pointers to it,
        // so it can now be destroyed

        // first check whether the object has some children
        // (in the form of leftID and rightID), and if so
        // first call decreaseParentCnt for them recursively
        // (here we can also use `ptr->opSymbol != ""` 
        // or `ptr->left != nullptr` or `ptr->right != nullptr`)
        if(ptr->leftID != nullptr and ptr->rightID != nullptr) {
            auto it = Parent.find({ptr->opSymbol, ptr->leftID, ptr->rightID});
            Parent.erase(it);
            decreaseParentCnt(ptr->leftID);
            decreaseParentCnt(ptr->rightID);
        }

        // free the memory allocated to object pointed by ptr
        // and set it to nullptr
        delete(ptr);
        ptr = nullptr;
    }

    /**
     * @brief Function to increment parentCnt of an IDstruct object.
     * 
     * @param[in]  ptr  Pointer to the IDstruct object
     * @returns    Void
     * 
     * @see IDstruct
     **/
    void increaseParentCnt(IDstruct *&ptr) {
        if(ptr == nullptr) return;
        ptr->parentCnt += 1;
    }

    /**
     * @brief Creates a copy of a partition.
     * 
     * @details
     *  First call decreaseParentCnt for each element
     *  of the old partition, because we are removing references
     *  to them. Copy the required partition and then call 
     *  increaseParentCnt for each member of the copied
     *  partition, because we are adding a new reference 
     *  to them during copying.
     * 
     * @param[in, out]  oldPart  Old partition
     * @param[in]       toCopy   Partition to be copied
     * @returns         Void
     * 
     * @see IDstruct, Partition, partitions
     **/
    void copyPartition(Partition &oldPart, Partition const &toCopy) {
        // call decreaseParentCnt for each reference in the 
        // old partition because we would be removing them
        for(auto &el : oldPart)
            decreaseParentCnt(el);

        // now copy the partition
        oldPart = toCopy;

        // call increaseParentCnt for each reference in the
        // new copied partition (in oldPart) becase we have
        // created a new pointer reference to them
        for(auto &el : oldPart)
            increaseParentCnt(el);
    }

    /**
     * @brief 
     *  Returns pointer to IDstruct object that represents
     *  equivalence class corresponding to expression
     *  "left op right" in a given partition.
     * 
     * @details
     *  If such an object doesn't exists it creates one
     *  such and updates other required things.
     * 
     * @param[in]   curPart Partition under consideration
     * @param[in]   op      Operator
     * @param[in]   left    Left operand
     * @param[out]  right   Right operand
     * @returns     Pointer to IDstruct object representing
     *              "left op right" in curPart
     * 
     * @see IDstruct, Partition, partitions
     **/
    IDstruct* findIDstrct(Partition const &curPart, 
                          char op, Value *left, Value *right) {
                              
        IDstruct *leftID = curPart[indexCV[left]];
        IDstruct *rightID = curPart[indexCV[right]];

        // the required IDstruct object exists, return
        // a pointer to it
        auto it = Parent.find({op, leftID, rightID});
        if(it != Parent.end()) return (it->second);

        // the required IDstruct object doesn't exists,
        // create a new one and return a pointer to it
        IDstruct *ptr = new IDstruct(op, leftID, rightID);
        Parent[{op, leftID, rightID}] = ptr;
        increaseParentCnt(leftID), increaseParentCnt(rightID);
        
        return ptr;
    }

    /**
     * @brief 
     *  Updates Predecessors map and confluencePoints set
     *  for instructions in a function.
     * 
     * @param[in]   F     Function under consideration
     * @returns     Void 
     * 
     * @see Predecessors, confluencePoints
     **/
    void findPredecessors(Function &F) {
        // first clear the variables, to remove any previous contents
        Predecessors.clear();
        confluencePoints.clear();

        // prevInst will store the previous instruction
        // processed, so when we are not at the beginning
        // of a basic block, prevInst will be the predecessor
        // of current instruction.
        Instruction *prevInst = nullptr;

        for(Instruction &I : instructions(F)) {
            BasicBlock *BB = I.getParent();
            
            if(&BB->front() == &I) {
                // if we are at the beginnning of a basic block
                // traverse through the predecessor basic blocks -
                // their last instructions would be predecessors
                // of current instruction
                vector<Instruction *> pred = {};
                for(auto it = pred_begin(BB); it != pred_end(BB); it++)
                    pred.push_back(&(*it)->back());

                Predecessors.emplace(&I, pred);
                confluencePoints.insert(&I);
            } else Predecessors.emplace(&I, vector<Instruction *>({prevInst}));

            prevInst = &I;
        }
    }

    /**
     * @brief Transfer function.
     * 
     * @param[in, out]  curPart   Partition at current program point
     * @param[in]       prevPart  Partition at predecessor program
     *                            point
     * @param[in]       I         Current instruction
     * @returns         Void
     *  
     * @see Partition, partitions
     **/
    void transferFunction(Partition &curPart, Partition const &prevPart, 
                          Instruction &I) {
                             
        // first copy the partition at predecessor instruction
        // and then make required changes
        copyPartition(curPart, prevPart);
        
        // changed stores the variable whose equivalence has
        // been changed
        Value *changed = nullptr;
        
        Value *left, *right;
        IDstruct *leftID, *rightID, *IID, *newID;

        if(isa<LoadInst>(I)) {
            left = I.getOperand(0);
            leftID = curPart[indexCV[left]];
            IID = curPart[indexCV[&I]];

            decreaseParentCnt(IID);
            curPart[indexCV[&I]] = leftID;
            increaseParentCnt(leftID);

            changed = &I;
        } else if(isa<StoreInst>(I)) {
            left = I.getOperand(0);
            right = I.getOperand(1);
            leftID = curPart[indexCV[left]];
            rightID = curPart[indexCV[right]];

            decreaseParentCnt(rightID);
            curPart[indexCV[right]] = leftID;
            increaseParentCnt(leftID);

            changed = right;
        } else if(isa<BinaryOperator>(&I)) {
            left = I.getOperand(0);
            right = I.getOperand(1);
            IID = curPart[indexCV[&I]];
            char op = getOpSymbol(I.getOpcodeName());
            newID = findIDstrct(curPart, op, left, right);

            decreaseParentCnt(IID);
            curPart[indexCV[&I]] = newID;
            increaseParentCnt(newID);

            changed = &I;
        } else if(isa<CallInst>(I)) {
            IID = curPart[indexCV[&I]];
            newID = new IDstruct;

            decreaseParentCnt(IID);
            curPart[indexCV[&I]] = newID;
            increaseParentCnt(newID);

            changed = &I;
        }

        // if no variable has been changed then return,
        // otherwise update equivalence information for
        // every two operand expression containing the 
        // changed variable
        if(changed == nullptr) return;
        for(auto op : Ops)
            for(auto x : CuV) {
                decreaseParentCnt(curPart[indexExp[{op, x, changed}]]);
                newID = findIDstrct(curPart, op, x, changed);
                curPart[indexExp[{op, x, changed}]] = newID;
                increaseParentCnt(newID);

                decreaseParentCnt(curPart[indexExp[{op, changed, x}]]);
                newID = findIDstrct(curPart, op, changed, x);
                curPart[indexExp[{op, changed, x}]] = newID;
                increaseParentCnt(newID);
            }
    }

    /**
     * @brief Confluence function.
     * 
     * @param[in]   I          Current instruction
     * @param[out]  partition  Partition at the confluence point
     * 
     * @see Partition, partitions
     **/
    void confluenceFunction(Instruction &I, Partition &partition) {
        vector<Instruction *> &predecessors = Predecessors[&I];

        if(predecessors.empty()) findInitialPartition(partition);
        else {
            // vector to check which constant/variable has already 
            // been considered while processing
            vector<bool> accessFlag(CuV.size(), false);

            // initialize partition vector
            partition.assign(numExps, nullptr);

            // process each constant/variable that has not
            // already been considered
            for(auto el : indexCV) {
                if(not accessFlag[el.second]) {
                    accessFlag[el.second] = true;
                    
                    // check if the variable/constant is equivalent on all
                    // the predecessors, if so make it point to the same
                    // IDstruct object for the confluence partition. Notice 
                    // that this case will always be true for a constant.
                    // Also another thing is if in a partition, the variable/
                    // constant contains nullptr we have to just ignore it,
                    // as theoretically it means that the corresponding partition 
                    // represents the top element
                    bool flag = true;
                    IDstruct *ptr = nullptr;
                    for(int i = 0; i < (int)predecessors.size() and flag; i++) {
                        IDstruct *nptr = partitions[predecessors[i]][el.second];
                        if(nptr == nullptr) continue;
                        else if(ptr == nullptr) ptr = nptr;
                        else if(ptr != nptr) flag = false;
                    }

                    if(flag) {
                        partition[el.second] = ptr;
                        increaseParentCnt(ptr);
                    } else {
                        // however if the previous case is not
                        // true, we must create a new IDstruct
                        // object to represent the class of 
                        // the varible in the temporary partition.
                        // Also, every other variable that is 
                        // equivalent (belong to the same class)
                        // to the current variable at all the
                        // predecessors must also point to the same
                        // IDstruct object
                        set<Value *> setCV, intersection(CuV);
                        set<expTuple> setExp;
                        for(auto inst : predecessors) {
                            getClass(el.second, partitions[inst], setCV, setExp);
                            setIntersect<Value *>(intersection, setCV);
                        }

                        IDstruct *ID = new IDstruct();
                        for(auto el_ : intersection) {
                            accessFlag[indexCV[el_]] = true;
                            partition[indexCV[el_]] = ID;
                            increaseParentCnt(ID);
                        }
                    }
                }
            }

            // now consider every two operand expression
            for(auto el : indexExp) {
                Value *left = get<1>(el.first);
                Value *right = get<2>(el.first);
                char op = get<0>(el.first);

                // find ID struct object coressponding to the expression
                IDstruct *ID = findIDstrct(partition, op, left, right);
                partition[el.second] = ID;
                increaseParentCnt(ID);
            }
        }
    }

    /**
     * @brief Prints a constant/variable in readable form.
     * 
     * @param[in]   value   LLVM representation of constant/variable
     * @returns     Void
     **/
    void printCV(Value const *value) {
        // if the argument is a constant print its value
        // else it is a variable - print its assigned name
        if(dyn_cast<ConstantInt>(value))
            errs() << dyn_cast<ConstantInt>(value)->getValue().toString(10, true);
        else errs() << value->getName();
    }

    /**
     * @brief Prints a set of constant/variable in readable form.
     * 
     * @param[in]   setCV   Set of constants/variables
     * @returns     Void
     **/
    void printSetCV(set<Value *> const &setCV) {
        for(auto el : setCV)
            printCV(el), errs() << ", ";
        errs() << "\n";
    }

    /**
     * @brief Prints a two operand expression in readable form.
     * 
     * @param[in]   exp  Expression to be printed
     * @returns     Void
     * 
     * @see expTuple
     **/
    void printExp(expTuple const &exp) {
        // first print left operand, then the operator and finally
        // the right operand
        printCV(get<1>(exp));
        errs() << " " << get<0>(exp) << " ";
        printCV(get<2>(exp));
    }

    /**
     * @brief Prints a set of two operand expressions in readable form.
     * 
     * @param[in]   value   Set of expressions to be printed
     * @returns     Void
     **/
    void printSetExp(set<expTuple> const &setExp) {
        for(auto el : setExp)
            printExp(el), errs() << ", ";
        errs() << "\n";
    }

    /**
     * @brief Prints a partition in readable format.
     * 
     * @param[in]   partition   Partition to be printed
     * @returns     Void
     *
     * @see Partition
     **/
    void printPartition(Partition const &partition) {
        // variables to hold an equivalence class
        set<Value *> setCV;
        set<expTuple> setExp;

        // to check whether an expression has already been 
        // considered
        vector<bool> done(numExps, false);

        // lambda function to print an equivalence class
        // (setCV and setExp)
        auto lambda = [&]() {
            errs() << "{";
            
            for(auto it = setCV.begin(); it != setCV.end();) {
                printCV(*it);
                done[indexCV[*it]] = true;
                if(++it != setCV.end()) errs() << ", ";
            }

            if(!setCV.empty() and !setExp.empty()) errs() << ", ";

            for(auto it = setExp.begin(); it != setExp.end();) {
                printExp(*it);    
                done[indexExp[*it]] = true;
                if(++it != setExp.end()) errs() << ", ";
            }

            errs() << "}, ";
        };

        // print equivalence class of each constant/variable
        for(auto el : indexCV) {
            if(done[el.second]) continue;
            getClass(el.second, partition, setCV, setExp);
            lambda();
        }

        // print equivalence class of each two operand expression
        for(auto el : indexExp) {
            if(done[el.second]) continue;
            getClass(el.second, partition, setCV, setExp);
            lambda();
        }
    }

    /**
     * @brief Prints the llvm source code.
     * 
     * @param[in]   F     Function that has to be printed
     * @returns     Void
     **/
    void printCode(Function &F) {
        for(auto BB = F.begin(); BB != F.end(); BB++) {
            errs() << "BasicBlock: " << BB->getName() << "\t\t[Predecessors: ";
            for (auto it = pred_begin(&*BB); it != pred_end(&*BB); it++)
                errs() << (*it)->getName() << " ";
            errs() << "]\n";

            for(auto I = BB->begin(); I != BB->end(); I++)
                errs() << (*I) << "\n";
            errs() << "\n";
        }
    }

    /**
     * @brief 
     *  Assigns name to basic blocks and variables for 
     *  easy reference.
     * 
     * @param[in]  F    Function block over which we are operating
     * @returns    Void
     **/
    void assignNames(Function &F) {
        int BBCtr = 1, varCtr = 1;

        for(auto BB = F.begin(); BB != F.end(); BB++) {
            // first assign name to the basic block
            BB->setName("BB" + to_string(BBCtr++));

            // now assign name to all non-void instructions
            for(auto I = BB->begin(); I != BB->end(); I++)
                if(!I->getType()->isVoidTy())
                    I->setName("T" + to_string(varCtr++));
        }
    }

    /**
     * @brief 
     *  Finds herbrand equivalence information by updating
     *  partitions map, for a given function.
     * 
     * @param[in]  F    Function under consideration
     * @returns    Void
     * 
     * @see partitions
     **/
    void findHerbrandEquivalence(Function &F) {
        if(DEBUG) {
            PRINT("Herbrand Equivalence Computation");
            errs() << "\n\n";
        }

        // first clear the map, to remove any previous contents
        partitions.clear();

        // for each instruction, initialise its partition to be the
        // one in which every element is assigned to nullptr.
        // This is equivalent to the top element theoretically.
        for(Instruction &I : instructions(F)) 
            partitions.emplace(&I, Partition(numExps, nullptr));

        // variable to check for convergence
        bool converged = false;
        // iteration counter
        int iterationCtr = 1;

        while(not converged) {
            if(DEBUG) PRINT("Iteration " + to_string(iterationCtr++));

            converged = true;

            for(Instruction &I : instructions(F)) {
                // store old partition to check for changes
                // in the convergence information
                Partition oldPartition = partitions[&I];

                if(confluencePoints.find(&I) != confluencePoints.end()) {
                    // if the instruction is at a confluence point

                    // first find the partition at the confluence,
                    // this would be a intermediate information needed
                    Partition confluencePartition;
                    confluenceFunction(I, confluencePartition);

                    if(DEBUG) {
                        // parent basic block of current instruction
                        BasicBlock *BB = I.getParent();
                        
                        errs() << "Start of basic block " << BB->getName();
                        errs() << "\t\t[Confluence of ";
                        for (auto it = pred_begin(BB); it != pred_end(BB); it++)
                            errs() << (*it)->getName() << " ";
                        errs() << "]\n";
                        printPartition(confluencePartition);
                        errs() << "\n\n";
                    }
                    
                    // now apply transfer function with confluencePartition
                    // as predecessor partition
                    transferFunction(partitions[&I], confluencePartition, I);

                    // remove any reference information from confluencePartition
                    for(auto el : confluencePartition)
                        decreaseParentCnt(el);
                } else { 
                    // else the instruction is at a transfer point
                    Instruction *prevInst = Predecessors[&I][0];
                    transferFunction(partitions[&I], partitions[prevInst], I);
                }

                if(DEBUG) {
                    errs() << I << "\n";
                    printPartition(partitions[&I]);
                    errs() << "\n\n";
                }

                // update convergence flag
                if(not samePartition(oldPartition, partitions[&I])) 
                    converged = false;
            }
        }

        if(DEBUG) errs() << "\n\n";
    }

    /**
     * @brief 
     *  Finds available variables at each instruction
     *  of a function.
     * 
     * @details
     *  Calculation of \c availVariables is a forward dataflow problem, 
     *  calculated via fixed point computation. A variable is available 
     *  at a point if all paths from the start of the program to that 
     *  point contains a definition of the variable. Our universe is
     *  <b> U - the set of program variables </b> and for each program 
     *  point <b> P </b>, we initialise <b> OUT[P] </b> to 
     *  <b> U </b>. Also, let <b> GEN[P] </b> to be the variable 
     *  defined at <b> P </b> if any, otherwise empty set. We will perform 
     *  updates as \n \n
     *  <tt> IN[P] = intersect(availVariables[P']) </tt>
     *  <tt> OUT[P] = union(IN[P], GEN[P]) </tt>
     *  \n \n where \c union is normal set union and \c intersect is 
     *  performed over all <b> P' </b> belonging to the predecessor of 
     *  <b> P </b>. Finally, <tt> availVariables[P] = OUT[P] </tt>. \n
     *  For the purpose of implementation, IN and OUT will not be maintained
     *  explicitly, but the same information will be kept in availVariables 
     *  itself.
     * 
     * @param[in]  F    Function under consideration
     * @returns    Void
     * 
     * @note
     *  Available Variables is different from both available expressions 
     *  and reaching definition.
     * 
     * @see availVariables
     **/
    void findAvailableVariables(Function &F) {
        if(DEBUG) {
            PRINT("Available Variable Computation");
            errs() << "\n\n";
        }

        // initialise availVariables for each instruction
        for(Instruction &I : instructions(F)) 
            availVariables.emplace(&I, Variables);

        // variable to check for convergence
        bool converged = false;
        // iteration counter
        int iterationCtr = 1;

        while(not converged) {
            if(DEBUG) PRINT("Iteration " + to_string(iterationCtr++));

            converged = true;

            for(Instruction &I : instructions(F)) {
                // store old availVariable information to check
                // for convergence
                set<Value *> oldAvail = availVariables[&I];

                // first find IN of current instruction and store it
                // in availVariables[&I]
                if(confluencePoints.find(&I) != confluencePoints.end()) {
                    // if the instruction is not a transfer point

                    vector<Instruction *> &predecessors = Predecessors[&I];

                    if(predecessors.empty()) {
                        // if current instruction has no predecessors -
                        // it means either the instruction is the first
                        // instruction in the program or is the first
                        // instruction in an unreachable basic block
                        availVariables[&I].clear();
                    } else {
                        // if the current instruction is a confluence point
                        // find the intersection of availVariables at all the
                        // predecessors
                        availVariables[&I] = Variables;
                        for(auto el : predecessors)
                            setIntersect<Value *>(availVariables[&I], availVariables[el]);
                    }
                } else {
                    // if the current instruction is a transfer point
                    Instruction *prevInst = Predecessors[&I][0];
                    availVariables[&I] = availVariables[prevInst];
                }

                if(DEBUG) {
                    errs() << I << "\n\tIN: ";
                    printSetCV(availVariables[&I]);
                }
                
                // now update IN information in availVariables[&I] to
                // contain OUT. For the purpose of LLVM implementation
                // variables defined by alloca instruction should not
                // be considered for available Variables computation
                if(not I.getType()->isVoidTy() and not isa<AllocaInst>(&I))
                    availVariables[&I].insert(&I);

                if(DEBUG) {
                    errs() << "\tOUT: ";
                    printSetCV(availVariables[&I]);
                    errs() << "\n";
                }

                // update convergence flag
                if(oldAvail != availVariables[&I]) converged = false;
            }

            if(DEBUG) errs() << "\n\n";
        }
    }

    /**
     * @brief 
     *  Function to remove redundant instructions by using
     *  herbrand equivalence and available expression information.
     * 
     * @param[in]   F     Function that has to be printed
     * @returns     Void
     **/
    void removeRedundantExpressions(Function &F) {
        if(DEBUG) {
            PRINT("Removing Redundant Instructions");
            errs() << "\n\n";
        }

        // set to store deleted variables and their names
        set<Value *> deletedVars;
        set<string> deletedVarsName;

        // iterate over instructions removing redundant ones
        auto insts = instructions(F);
        for(auto itr = insts.begin(); itr != insts.end();) {
            Instruction *I = &(*itr++);
            
            if(DEBUG) errs() << (*I) << "\n";

            // index corresponding to RValue of current instruction
            int index;
            if(isa<LoadInst>(I)) index = indexCV[I->getOperand(0)];
            else if(isa<BinaryOperator>(I)) {
                Value *left = I->getOperand(0);
                Value *right = I->getOperand(1);
                char op = getOpSymbol(I->getOpcodeName());
                index = indexExp[{op, left, right}];
            } else {
                // if there is no RValue skip current instruction
                if(DEBUG) errs() << "  => Instruction skipped\n\n\n";
                continue;
            }

            // get herbrand class of RValue at current program point
            set<Value *> setCV;
            set<expTuple> setExp;
            getClass(index, partitions[I], setCV, setExp);

            if(DEBUG) {
                errs() << "\tsetCV: ", printSetCV(setCV);
                errs() << "\tsetExp: ", printSetExp(setExp);
                errs() << "\tAvailable: ", printSetCV(availVariables[I]);

                errs() << "\tDeleted Variables: ";
                for(auto el : deletedVarsName) 
                    errs() << el << ", ";
                errs() << "\n";
            }

            // value with which to replace current instruction
            Value *replacement = nullptr;

            // first check if there is some constant in the Herbrand
            // class of RValue at current program point, if not then
            // check for some available variable. If one of the two 
            // is found replace current instruction with it.
            for(auto el : Constants)
                if(setCV.find(el) != setCV.end()) {
                    replacement = el;
                    break;
                }
            if(replacement == nullptr) {
                for(auto el : availVariables[I])
                    if(el != I and setCV.find(el) != setCV.end() 
                               and deletedVars.find(el) == deletedVars.end()) {
                        replacement = el;
                        break;
                    }
            }

            // if a replacement is found delete current instruction
            // and replace all its uses
            if(replacement != nullptr) {
                deletedVars.insert(I);
                deletedVarsName.insert(I->getName());
                I->replaceAllUsesWith(replacement);
                I->eraseFromParent();

                if(DEBUG) {
                    errs() << "  => Instruction deleted: ";
                    printCV(replacement);
                    errs() << "\n";
                }
            } else {
                if(DEBUG) errs() << "  => Instruction not deleted\n";
            }
            
            if(DEBUG) errs() << "\n\n";
        }
    }

    /**
     * @brief Body of the pass
     **/
    struct HerbrandEquivalence : public FunctionPass {
        // LLVM uses ID’s address to identify a pass
        static char ID;

        // constructor - calls base class constructor
        HerbrandEquivalence() : FunctionPass(ID) {}

        // the function pass
        bool runOnFunction(Function &F) override {
            // clear the contents of global variables
            Constants.clear(), CuV.clear(), Variables.clear();
            indexCV.clear(), indexExp.clear();
            partitions.clear(), Parent.clear();
            Predecessors.clear(), confluencePoints.clear();
            availVariables.clear();

            // assign indexes to constants, variables and two operand 
            // expressions; assign names to variables; fill Predecessors
            // map; print renamed code
            assignIndex(F), assignNames(F);
            findPredecessors(F);
            if(DEBUG) {
                PRINT("Renamed Code"), printCode(F);
                errs() << "\n\n";
            }

            findHerbrandEquivalence(F);
            findAvailableVariables(F);
            removeRedundantExpressions(F);

            if(DEBUG) PRINT("Optimised Code"), printCode(F);

            // call decreaseParentCnt to free dynamically allocated memory
            for(auto partition : partitions)
                for(auto el : partition.second)
                    decreaseParentCnt(el);

            // return true, because the pass is making changes
            // in the input file
            return true;
        }
    };
}

// initialse static variable ID and register the pass
char HerbrandPass::HerbrandEquivalence::ID = 0;
static RegisterPass<HerbrandPass::HerbrandEquivalence> 
        Pass("HerbrandEquivalence", "Herbrand equivalence analysis");
