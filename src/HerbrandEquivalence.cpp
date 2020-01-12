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
 * Anonymous namespace containing the LLVM pass 
 **/
namespace HerbrandPass {

    /** 
     * @struct IDstruct
     * @brief Structure for capturing Herbrand Equivalence classes.
     * 
     * @details 
     *  At a given program point, expressions pointing to the same 
     *  IDstruct object are Herbrand Equivalent at that point.
    **/
    struct IDstruct {
        /**
         * @brief 
         *  Operator of the Herbrand class represented by the object.
         * 
         * @details
         *  For a constant or variable it is just empty string. 
         *  For compound expressions, it represents the operator 
         *  which combines the left and right subexpressions.
         **/
        string opSymbol;

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
        IDstruct() : opSymbol(""), parentCnt(0), leftID(nullptr), rightID(nullptr) {}

        /**
         * @brief Parameterised constructor for the structure.
         * 
         * @param   op      Operator for the current Herbrand Equivalence class
         * @param   leftID  Pointer to the class corresponding to left subexpression
         * @param   rightID Pointer to the class corresponding to right subexpression
         **/
        IDstruct(string op, IDstruct *leftID, IDstruct *rightID) {
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
    typedef tuple<string, Value *, Value *> expTuple;

    /**
     * @brief
     *  Vector to hold equivalence information for each
     *  constant/variable/two operand expression at a
     *  program point
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
    set<string> Ops({"+"});

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
     *  Stores Herbrand Equivalence information for each instruction
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
    map<tuple<string, IDstruct *, IDstruct *>, IDstruct *> Parent;

    /**
     * @brief 
     *  Returns operator symbol corresponding to the 
     *  output of llvm::Instruction::getOpcodeName function.
     * 
     * @param[in]   opCodeName  A string returned by getOpcodeName
     * @returns     The symbol corresponding to the operator name
     * 
     * @see llvm::Instruction::getOpcodeName
     **/
    string getOpSymbol(string opCodeName) {
        if(opCodeName == "add") return "+";
        if(opCodeName == "sub") return "-";
        if(opCodeName == "mul") return "*";
        if(opCodeName == "sdiv") return "/";
        if(opCodeName == "udiv") return "/";
        return "";
    }

    /**
     * @brief 
     *  Initialises Constants, Variables, CuV, indexExp and indexCV
     *  by looking through the instructions in the program.
     * 
     * @note
     *  The function also assigns names to temporaries for further 
     *  reference (T1, T2, T3, ...).
     * 
     * @param[in]   F     Function block over which we are operating
     * @returns     Void
     * 
     * @see Constants, CuV, indexCV, indexExp, Variables
     **/
    void assignIndex(Function &F) {
        // delete the contents of global variables
        Variables.clear(), Constants.clear(), CuV.clear();
        indexCV.clear(), indexExp.clear();

        int ctr = 1;

        // first find the set of constants and variables
        // by iterating over the instructions
        for(Instruction &I : instructions(&F)) {
            // if the instruction is not of void type then 
            // it represents a variable. Also we assign 
            // a name to the variable for further reference
            if(!I.getType()->isVoidTy()) {
                Variables.insert(&I);
                I.setName("T" + to_string(ctr++));
            }

            // now we iterate over its operands, however
            // we should skip alloca instruction as its
            // operands doesn't contain any constant/variable
            // that concerns us
            if(isa<AllocaInst>(&I)) continue;
            for(int i = 0; i < (int)I.getNumOperands(); i++) {
                Value *value = I.getOperand(i);
                if(dyn_cast<Constant>(value)) {
                    Constants.insert(value);
                } else {
                    // ideally the else part should not be required
                    Variables.insert(value);
                    if(not value->hasName()) {
                        value->setName("T" + to_string(ctr++));
                    }
                }
            }
        }
        CuV = Constants, CuV.insert(Variables.begin(), Variables.end());

        // now assign integer indexes to variables, constants
        // and two operand instructions
        ctr = 0;
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
        partition.clear(), partition.resize(numExps);

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
            if(setCVfirst != setCVsecond || setExpfirst != setExpsecond)
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
            if(setCVfirst != setCVsecond || setExpfirst != setExpsecond)
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
        if(ptr->leftID != nullptr && ptr->rightID != nullptr) {
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
                          string op, Value *left, Value *right) {
                              
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
     * @brief Finds predecessors of an instruction
     * 
     * @param[in]   I     Instruction under consideration
     * @param[out]  pred  List of predecessor instructions
     * @returns     Void 
     **/
    void findPredecessors(Instruction &I, vector<Instruction *> &pred) {
        pred.clear();
        for(auto it = pred_begin(I.getParent()); it != pred_end(I.getParent()); it++)
            pred.push_back(&(*it)->back());
    }

    /**
     * @brief Transfer function
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
        } else if(I.isBinaryOp()) {
            left = I.getOperand(0);
            right = I.getOperand(1);
            IID = curPart[indexCV[&I]];
            string op = getOpSymbol(I.getOpcodeName());
            newID = findIDstrct(curPart, op, left, right);

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
     * @brief Confluence function
     * 
     * @param[in, out]  partition  Existing partition at current 
     *                             instruction
     * @param[in]       I          Current instruction
     * 
     * @see Partition
     **/
    void confluenceFunction(Partition &partition, Instruction &I) {

        // first find the predecessors of current instruction
        vector<Instruction *> predecessors;
        findPredecessors(I, predecessors);

        // temporary partition to represent equivalence classes at
        // the confluence point, two expressions are equivalent
        // iff they are equivalent for all the predecessors
        Partition tempPartition(numExps, nullptr);

        vector<bool> accessFlag(CuV.size(), false);
        for(auto el : indexCV) {
            if(not accessFlag[el.second]) {
                accessFlag[el.second] = true;
                bool flag = true;

                for(int i = 1; i < (int)predecessors.size() && flag; i++)
                    if(partitions[predecessors[0]][el.second] != partitions[predecessors[i]][el.second])
                        flag = false;

                if(flag) tempPartition[el.second] = partitions[predecessors[0]][el.second];
                else {
                    set<Value *> vV, intersection(CuV);
                    set<expTuple> vTup;
                    for(auto inst : predecessors) {
                        getClass(el.second, partitions[inst], vV, vTup);
                        setIntersect<Value *>(intersection, vV);
                    }

                    IDstruct *ID = new IDstruct();
                    for(auto nel : intersection) {
                        accessFlag[indexCV[nel]] = true;
                        tempPartition[indexCV[nel]] = ID;
                    }
                }
            }
        }

        for(auto el : indexExp) {
            Value *left = get<1>(el.first), *right = get<2>(el.first);
            string op = getOpSymbol(get<0>(el.first));
            tempPartition[el.second] = findIDstrct(tempPartition, op, left, right);
        }

        transferFunction(partition, tempPartition, I);
    }

    /**
     * @brief Prints a constant/variable in readable form
     * 
     * @param[in]   value   LLVM representation of constant/variable
     * @returns     Void
     **/
    void printCV(Value const *value) {
        // if the argument is a constant print its value
        // else it is a variable - print its assigned name
        if(dyn_cast<Constant>(value))
            errs() << dyn_cast<ConstantInt>(value)->getValue().toString(10, true);
        else errs() << value->getName();
    }

    /**
     * @brief Prints a two operand expression in readable form
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
     * @brief Prints a partition in readable format
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

            if(!setCV.empty() && !setExp.empty()) errs() << ", ";

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
     * @brief Body of the pass
     **/
    struct HerbrandEquivalence : public FunctionPass {
        // LLVM uses ID’s address to identify a pass
        static char ID;

        // constructor - calls base class constructor
        HerbrandEquivalence() : FunctionPass(ID) {}

        // the function pass
        bool runOnFunction(Function &F) override {
            // first assign indexes to constants/variables/
            // two operand expressions
            assignIndex(F);

            // a lambda function for printing header
            auto print = [](string x) {
                errs() << string(150, '=') << "\n" << x << "\n"
                       << string(150, '=') << "\n";
            };

            // find and print initial partition
            Partition initialPartition;
            findInitialPartition(initialPartition);
            print("Initial Partition");
            printPartition(initialPartition);
            errs() << "\n\n";

            // for each instruction, initialise its partition to be the
            // in which every element is in a different partition
            for(Instruction &I : instructions(F)) 
                findInitialPartition(partitions[&I]);

            // variable to check for convergence
            bool converged = false;
            // iteration counter
            int iterationCtr = 1;
            while(not converged) {
                print("Iteration " + to_string(iterationCtr++));

                converged = true;
                Instruction *prevInst = nullptr;

                for(Instruction &I : instructions(F)) {
                    // if(partitions.find(&I) == partitions.end()) partitions[&I] = Partition(numExps, nullptr);

                    // store old partition to check for changes
                    // in the convergence information
                    Partition oldPartition = partitions[&I];

                    // for the first instruction use initialPartition
                    // as the partition for its predecessor
                    if(prevInst == nullptr)
                        transferFunction(partitions[&I], initialPartition, I);
                    // if the instruction is at a confluence point
                    else if(&I.getParent()->front() == &I)
                        confluenceFunction(partitions[&I], I);
                    // else the instruction is at a transfer point
                    else transferFunction(partitions[&I], partitions[prevInst], I);

                    errs() << I << "\n";
                    printPartition(partitions[&I]);
                    errs() << "\n\n";

                    // update prevInst and convergence flag
                    prevInst = &I;
                    if(not samePartition(oldPartition, partitions[&I])) 
                        converged = false;
                }
            }

            // return false, because the pass doesn't make any changes
            // in the input file but just analyses it
            return false;
        }
    };
}

// initialse static variable ID and register the pass
char HerbrandPass::HerbrandEquivalence::ID = 0;
static RegisterPass<HerbrandPass::HerbrandEquivalence> 
        Pass("HerbrandEquivalence", "Herbrand equivalence analysis");
