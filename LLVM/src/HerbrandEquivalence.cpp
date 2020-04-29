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

// Macro to print a header 
#define PRINT_HEADER(str) errs() << std::string(100, '=') << "\n" << (str) \
                                 << "\n" << std::string(100, '=') << "\n"

// If DEBUG flag is 0, the pass won't print any debug information
// else it will
#define DEBUG 1

// ExpressionTy value for a constant or a variable
#define EXP(x) {'\0', (x), nullptr}

/**
 * @brief 
 *  Generic function to find the intersection 
 *  of two std::set.
 * 
 * @tparam          T     Any valid type for which std::set 
 *                        can be created
 * @param[in]       xset  Reference to first input set
 * @param[in, out]  yset  Reference to second input set
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
 *  Returns operator symbol corresponding to the 
 *  output of llvm::Instruction::getOpcodeName function.
 * 
 * @param[in]   opCodeName  A string returned by getOpcodeName
 * @returns     The character symbol corresponding to the operator name
 * 
 * @see     llvm::Instruction::getOpcodeName
 **/
char getOpSymbol(std::string opCodeName) {
    if(opCodeName == "add") return '+';
    if(opCodeName == "sub") return '-';
    if(opCodeName == "mul") return '*';
    if(opCodeName == "sdiv") return '/';
    if(opCodeName == "udiv") return '/';
    return '\0';
}

/** 
 * @namespace
 * Namespace containing the LLVM pass 
 **/
namespace HerbrandPass {

    /** 
     * @brief Set of constants used in the program.
     *
     * @note 
     *  Value is LLVM structure used for representing
     *  constants and variables.
     * 
     * @see     llvm::Value
     **/
    std::set<Value *> Constants;
    
    /** 
     * @brief Set of variables used in the program.
     *
     * @note 
     *  Value is LLVM structure used for representing
     *  constants and variables.
     * 
     * @see     llvm::Value
     **/
    std::set<Value *> Variables;

    /**
     * @brief Set of operators used in the program.
     * 
     * @note
     *  This set needs to be updated by adding operators
     *  if the programs on which the pass is being run 
     *  contains more operators.
     **/
    std::set<char> Ops({'+'});

    /** 
     * @brief 
     *  Represents an expression of length at most two.
     * 
     * @details 
     *  For two length expressions `x op y` the tuple 
     *  holds `{op, x, y}` whereas for a constant or
     *  variable `x` it holds `{'\0', x, nullptr}` 
     *  (here the first and last element of the tuple 
     *  have default value for consistency).
     *
     * @note 
     *  Value is LLVM structure used for representing
     *  constants and variables.
     * 
     * @see     llvm::Value
     **/
    typedef std::tuple<char, Value *, Value *> ExpressionTy;

    /**
     * @brief
     *  Maps expressions to integer indexes. This indexing is
     *  arbitrarily fixed in the beginning and used throughout 
     *  wherever indexing is required for the expressions.
     * 
     * @see     ExpressionTy, Partitions
     **/
    std::map<ExpressionTy, int> IndexExp;

    /**
     * @brief
     *  Counter to keep track of set identifiers. New set 
     *  identifiers are created by incrementing this counter.
     * 
     * @see     ExpressionTy, Partitions
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
     * @note
     *  For a normal program point the entries for expressions
     *  in the `Partitions` vector are non-negative. But, if the 
     *  partition represents TOP partition, the entries for all
     *  the expressions are -1. And the entry for any expression
     *  being -1 is sufficient to conclude that the partition
     *  vector represents TOP partition.
     * 
     * @see     IndexExp, SetCnt
     **/
    std::vector<std::vector<int>> Partitions;

    /**
     * @brief
     *  Map to hold parent set identifiers.
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
     * @note
     *  The two integers in the key of this map are ordered.
     *  Also this map is global used throughout the program.
     * 
     * @see     IndexExp, Partitions, SetCnt
     **/
    std::map<std::tuple<char, int, int>, int> Parent;

    /**
     * @struct 
     *  Represents a control flow graph node. There
     *  is a node for each program point, alongwith
     *  two special - START and END nodes.
     * 
     * @see     CFG, createCFG, llvm::Instruction
     **/
    struct CfgNodeTy {
        /**
         * @brief Type of control flow graph node
         * 
         * @details
         *  START and END are two special nodes to
         *  represent start and end point of a program.
         *  TRANSFER and CONFLUENCE corresponds to 
         *  whether the node represents a transfer or
         *  a confluence point.
         **/
        enum {START, END, TRANSFER, CONFLUENCE} NodeTy;

        /**
         * @brief
         *  Pointer to instruction corresponding to the
         *  node if it represents a transfer point, 
         *  otherwise holds `nullptr` for consistency.
         **/
        Instruction *instPtr;

        /**
         * @brief
         *  Vector of indexes of predecessor control flow
         *  graph nodes corresponding to `CFG` vector.
         * 
         * @details
         *  For START nodes this is empty, for TRANSFER nodes
         *  this is singular vector, for CONFLUENCE nodes this
         *  has more than one members and for END nodes this
         *  is non-empty.
         **/
        std::vector<int> predecessors;
    };

    /**
     * @brief
     *  Control flow graph corresponding to the program.
     * 
     * @details
     *  It contains nodes only corresponding to reachable
     *  instructions and confluence points along with two
     *  special START and END nodes.
     * 
     * @see     CfgNodeTy, createCFG
     **/
    std::vector<CfgNodeTy> CFG;

    /**
     * @brief Stores `CFG` index for each instruction.
     * 
     * @see     CFG, CfgNodeTy
     **/
    std::map<Instruction *, int> CfgIndex;

    /**
     * @brief 
     *  Assigns names to basic blocks and variables 
     *  for easy reference.
     * 
     * @param[in]  F    Function block over which 
     *                  we are operating
     * @returns    Void
     *
     * @note 
     *  These are not the names used in the original
     *  C/C++ source files.
     **/
    void assignNames(Function &F) {
        int BBCtr = 1, varCtr = 1;

        for(BasicBlock &BB : F) {
            // first assign name to the basic block
            BB.setName("BB" + std::to_string(BBCtr++));

            // now assign name to all non-void instructions
            for(Instruction &I : BB)
                if(not I.getType()->isVoidTy())
                    I.setName("T" + std::to_string(varCtr++));
        }
    }

    /**
     * @brief 
     *  Maps expressions of length atmost two to integers
     *  arbitrarily for indexing purpose by updating 
     *  `IndexExp` map. Also Initialises `Constants` and
     *  `Variables` by looking through the instructions in 
     *  the program.
     * 
     * @param[in]   F     Function block over which 
     *                    we are operating
     * @returns     Void
     * 
     * @see     Constants, IndexExp, Variables
     **/
    void assignIndex(Function &F) {
        ////////////////////////////////////////////////////
        // First update `Constants` and `Variables` sets by
        // iterating over instructions in the program
        ////////////////////////////////////////////////////
        
        for(Instruction &I : instructions(&F)) {
            // if the instruction is not of void type then 
            // it represents a variable. All the variables
            // will be covered in this case
            if(not I.getType()->isVoidTy()) 
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

        ////////////////////////////////////////////////////
        // Now assign indexes to expressions of length 
        // atmost two by updating `IndexExp` map
        ////////////////////////////////////////////////////

        // set to hold both constants and variables
        std::set<Value *> CuV = Constants;
        CuV.insert(Variables.begin(), Variables.end());

        int ctr = 0;
        for(auto el : CuV) IndexExp[EXP(el)] = ctr++;
        for(auto op : Ops)
            for(auto left : CuV)
                for(auto right : CuV)
                    IndexExp[{op, left, right}] = ctr++;
    }

    /**
     * @brief
     *  Creates control flow graph corresponding to
     *  the program.
     * 
     * @see     CfgNodeTy, CFG
     **/
    void createCFG(Function &F) {
        ////////////////////////////////////////////////////
        // First find the set of reachable basic blocks by
        // performing BFS from the starting basic block
        ////////////////////////////////////////////////////
        
        std::set<BasicBlock *> reachableBB;
        std::queue<BasicBlock *> q;

        // the list of basic blocks in the same order as
        // visited by BFS, for same traversal later again
        std::vector<BasicBlock *> bfsOrder;

        // mark the starting basic block as reachable
        // and also push it into the queue for BFS
        q.push(&F.front()), reachableBB.insert(&F.front());

        while(not q.empty()) {
            BasicBlock *bb = q.front();
            q.pop(), bfsOrder.push_back(bb);

            for(BasicBlock *nbb : successors(bb)) {
                if(reachableBB.find(nbb) == reachableBB.end())
                    reachableBB.insert(nbb), q.push(nbb);
            }
        }

        ////////////////////////////////////////////////////
        // Now again visit the basic blocks in the same 
        // order assigning an index for nodes in the control
        // flow graph vector `CFG` corresponding to
        // instructions and confluence points
        ////////////////////////////////////////////////////

        // some basic blocks might have just one
        // predecessor, in that case they don't require
        // a confluence node in the control flow graph.
        // This set keeps track of all the basic blocks
        // that require a confluence node
        std::set<BasicBlock *> confBlocks;

        // keeps track of current `CFG` index. Index 0 is
        // assigned to the starting node
        int curCfgIndex = 1;

        for(BasicBlock *bb : bfsOrder) {
            // number of predecessor basic blocks of `bb`,
            // which are reachable
            int predSz = 0;
            for(BasicBlock *nbb : predecessors(bb)) {
                if(reachableBB.find(nbb) != reachableBB.end())
                    predSz++;
            }

            // if this basic block requires a confluence node,
            // first assign an index to it
            if(predSz > 1)
                confBlocks.insert(bb), curCfgIndex++;
            
            // now assign nodes for the instructions in the
            // basic block
            for(Instruction &I : (*bb))
                CfgIndex[&I] = curCfgIndex++;
        }

        ////////////////////////////////////////////////////
        // Now actually create control flow graph nodes by
        // filling `CFG` vector. Note that the consistency
        // between `CfgIndex` and actual index of node for
        // instructions in `CFG` vector is implicitly 
        // maintained
        ////////////////////////////////////////////////////

        // START node
        CFG.push_back({CfgNodeTy::START, nullptr, std::vector<int>()});

        // `CFG` nodes that are predecessors of the special END
        // END instruction. Last instruction of the basic blocks 
        // that have no successors belong to this category. Only
        // if this vector is non-empty, an END node is created
        std::vector<int> predsEnd;

        for(auto bb : bfsOrder) {
            // holds index of predecessor nodes for instructions
            // in this block
            int predIndex;

            // if the block requires a confluence point,
            // first push it into the `CFG` vector
            if(confBlocks.find(bb) != confBlocks.end()) {
                std::vector<int> preds;
                for(BasicBlock *nbb : predecessors(bb)) {
                    if(reachableBB.find(nbb) != reachableBB.end())
                        preds.push_back(CfgIndex[&nbb->back()]);
                }
                CFG.push_back({CfgNodeTy::CONFLUENCE, nullptr, preds});

                // initialise `predIndex`
                predIndex = CFG.size() - 1;
            } else {
                // here only `predIndex` is 
                
                // this initialisation is for the first basic block
                // for which the `if` condition inside `for` never
                // becomes true
                predIndex = 0;

                for(BasicBlock *nbb : predecessors(bb))
                    if(reachableBB.find(nbb) != reachableBB.end()) {
                        // if `break` is removed, even then the `if` 
                        // block should be entered only once (except
                        // for the first basic block for which it is 
                        // entered never)
                        predIndex = CfgIndex[&nbb->back()];
                        break;
                    }
            }

            // now insert nodes corresponding to the instructions
            for(Instruction &I : (*bb)) {
                CFG.push_back({CfgNodeTy::TRANSFER, &I, 
                               std::vector<int>({predIndex})});

                // update `predIndex`
                predIndex = CFG.size() - 1;

                // this assertion must be passed for consistency
                // of this function
                assert(predIndex == CfgIndex[&I]);
            }

            if(succ_empty(bb)) predsEnd.push_back(CfgIndex[&bb->back()]);
        }

        // now create node corresponding to END, if required
        if(not predsEnd.empty()) {
            CFG.push_back({CfgNodeTy::END, nullptr, predsEnd});
        }
    }
    
    /**
     * @brief Prints a constant/variable in readable form.
     * 
     * @param[in]   value   LLVM representation of constant/variable
     * @returns     Void
     * 
     * @see     llvm::Value
     **/
    void printValue(Value const *value) {
        // if the argument is a constant print its value
        // else it is a variable - print its assigned name
        if(dyn_cast<ConstantInt>(value))
            errs() << dyn_cast<ConstantInt>(value)->getValue().toString(10, true);
        else errs() << value->getName();
    }

    /**
     * @brief Prints an expression in readable form.
     * 
     * @param[in]   exp  Expression to be printed
     * @returns     Void
     * 
     * @see     ExpressionTy
     **/
    void printExpression(ExpressionTy const &exp) {
        if(std::get<0>(exp)) {
            // if `exp` is a two length expression
            printValue(std::get<1>(exp));
            errs() << " " << std::get<0>(exp) << " ";
            printValue(std::get<2>(exp));
        } else {
            // if `exp` is just a constant/variable
            printValue(std::get<1>(exp));
        }
    }

    /**
     * @brief Prints the llvm source code.
     * 
     * @param[in]   F     Function that has to be printed
     * @returns     Void
     **/
    void printCode(Function &F) {
        PRINT_HEADER("LLVM CODE");
        errs() << "\n";

        for(BasicBlock &BB : F) {
            errs() << "BasicBlock: " << BB.getName();
            
            errs() << "\t\t[Predecessors:";
            for(BasicBlock *nbb : predecessors(&BB))
                errs() << ' ' << nbb->getName();
            errs() << "]\n";

            for(Instruction &I : BB)
                errs() << I << "\n";
            errs() << "\n";
        }

        errs() << "\n\n";
    }

    /**
     * @brief 
     *  Prints the control flow graph corresponding to 
     *  the program
     * 
     * @returns     Void
     * 
     * @see     CFG, CfgNodeTy
     **/
    void printCFG() {
        PRINT_HEADER("CONTROL FLOW GRAPH");
        errs() << "\n";

        for(int i = 0; i < (int)CFG.size(); i++) {
            CfgNodeTy &node = CFG[i];
            errs() << '[' << i << "] : ";

            switch(node.NodeTy) {
                case CfgNodeTy::START :
                    errs() << "START\n";
                    break;

                case CfgNodeTy::END :
                    errs() << "END  [Predecessors :";
                    for(auto el : node.predecessors)
                        errs() << ' ' << el;
                    errs() << "]\n";
                    break;

                case CfgNodeTy::TRANSFER :
                    errs() << "Transfer Point => [" 
                           << node.instPtr->getParent()->getName() 
                           << "]" << (*node.instPtr) << "\t[Predecessor : " 
                           << node.predecessors[0] << "]\n";
                    break;

                case CfgNodeTy::CONFLUENCE :
                    errs() << "Confluence Point => [Predecessors Nodes :";
                    for(auto el : node.predecessors)
                        errs() << ' ' << el << '('
                               << CFG[el].instPtr->getParent()->getName()
                               << ')';
                    errs() << "]\n";

                    break;
            }
        }
        errs() << "\n\n";
    }

    /**
     * @brief Prints a partition in readable format.
     * 
     * @param[in]   partition   The partition vector to 
     *                          be printed.
     * @return      Void
     * 
     * @see     IndexExp, Partitions
     **/
    void printPartition(std::vector<int> const &partition) {
        // if any index stores -1, then the whole vector
        // stores -1, representing the TOP element
        if(partition[0] == -1) {
            errs() << "<TOP ELEMENT>";
            return;    
        }

        int cnt = 0;
        // finding equivalent expressions in `mp` map
        std::map<int, std::vector<ExpressionTy>> mp;
        for(auto &el : IndexExp)
            mp[partition[el.second]].push_back(el.first), cnt++;

        // print the equivalence classes along with their
        // set identifiers
        for(auto &el : mp) {
            errs() << '[' << el.first << "]{";

            int sz = el.second.size();
            for(int i = 0; i < sz; i++) {
                printExpression(el.second[i]); 
                if(i != sz - 1) errs() << ", ";
            }

            errs() << "}, ";
        }
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
     * @return      Returns true if the two partitions are same,
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
     *  Returns set identifier for a given two length expression 
     *  at a program point.
     * 
     * @details
     *  With the help of `Parent` map, first a check is made if 
     *  a set identifier representing the expression at the
     *  current program point exists. If so it is returned else
     *  a new set identifier is created by updating `SetCnt` and
     *  returned. This information is also stored in the 
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
    int findSet(std::vector<int> const &partition, ExpressionTy const &exp) {
        // operator and set identifiers corresponding to left and right 
        // subexpressions at the current program point 
        char op = std::get<0>(exp);
        int leftSetId = partition[IndexExp[EXP(std::get<1>(exp))]];
        int rightSetId = partition[IndexExp[EXP(std::get<2>(exp))]];

        // checking if a set representing the expression
        // already exists
        std::tuple<char, int, int> tup = {op, leftSetId, rightSetId};
        auto it = Parent.find(tup);

        // if the set already exists return its identifier,
        // otherwise return new set identifier and update
        // `Parent` map with this information
        if(it == Parent.end()) return Parent[tup] = SetCnt++;

        return it->second;
    }

    /**
     * @brief Finds initial partition.
     * 
     * @details
     *  Finds a vector representing initial partition
     *  in which all expressions are in different 
     *  partition (has non-equivalent set identifiers).
     *  It also updates `Parent` map for set identifiers
     *  assigned to length two expressions.
     * 
     * @param[out]  partition   Vector to store the 
     *                          initial partition.
     * @return      Void
     * 
     * @see     findSet, IndexExp, Partitions
     **/
    void findInitialPartition(std::vector<int> &partition) {
        // create new IDstruct object for each epxression.
        // For length two expressions this is done indirectly
        // by calling `findSet` function which also updates
        // `Parent` map
        for(auto &el : IndexExp) {
            if(std::get<0>(el.first) == '\0') partition[el.second] = SetCnt++;
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
     *  to expressions (as given by `IndexExp`) which have same
     *  set identifiers as the index representing the expression 
     *  (passed as parameter) in the partition at the program point.
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
     * @see     IndexExp, Partitions
     **/
    void getClass(std::vector<int> const &partition, int expIdx, 
                  std::set<int> &expClass) {

        expClass.clear();
        int expSetId = partition[expIdx];

        for(int i = 0; i < (int)IndexExp.size(); i++) {
            if(expSetId == partition[i])
                expClass.insert(i);
        }
    }

    /**
     * @brief Transfer function associated with Herbrand analysis.
     * 
     * @param[in]   cfgIndex    Control flow graph node index on which 
     *                          transfer function is applied. The 
     *                          function modifies `Partitions[cfgIndex]`.
     * @returns     Void
     * 
     * @see     findSet, IndexExp, Partitions
     **/
    void transferFunction(int cfgIndex) {
        // current partition vector
        std::vector<int> &partition = Partitions[cfgIndex];

        // first copy predecessor partition into current partition
        partition = Partitions[CFG[cfgIndex].predecessors[0]];

        // if the current partition has any index with value -1, it 
        // means that it represents the TOP element and it has to be
        // left as such without any modifications
        if(partition[0] == -1) return;

        // if the node is the END CFG node, return
        if(CFG[cfgIndex].NodeTy == CfgNodeTy::END) return;

        // `changedExp` is expression (ie. a variable) which
        // has been assigned value and `changedToExp` is the 
        // expression which has been assigned to it. If 
        // operator in `changedToExp` is `#` - it symbolises
        // non-deterministic assignment
        Instruction *inst = CFG[cfgIndex].instPtr;
        ExpressionTy changedExp, changedToExp;

        if(isa<LoadInst>(inst)) {
            changedExp = EXP(inst);
            changedToExp = EXP(inst->getOperand(0));
        } else if(isa<StoreInst>(inst)) {
            changedExp = EXP(inst->getOperand(1));
            changedToExp = EXP(inst->getOperand(0));
        } else if(isa<BinaryOperator>(inst)) {
            char op = getOpSymbol(inst->getOpcodeName());
            Value *leftOp = inst->getOperand(0);
            Value *rightOp = inst->getOperand(1);
            changedExp = EXP(inst);
            changedToExp = {op, leftOp, rightOp};
        } else if(isa<CallInst>(inst)) {
            changedExp = EXP(inst);
            // here `#` symbolises non-deterministic assignment
            changedToExp = {'#', nullptr, nullptr};
        } else {
            // in this case the current instruction has not
            // modified the partition, so the function returns
            return;
        }

        if(std::get<0>(changedToExp) == '#') {
            // if it is a non-deterministic assignment, 
            // then create a new set identifier
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
            if(std::get<0>(el.first) == '\0') continue;
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
     * @see     findSet, IndexExp, Partitions
     **/
    void confluenceFunction(int cfgIndex) {
        // vector of  predecessor CFG node indexes
        std::vector<int> &predecessors = CFG[cfgIndex].predecessors;
        
        // if all the predecessors partition represents TOP 
        // element then their confluence is also TOP element.
        // So, the current partition need not be modified as
        // by initialisation the current partition should also
        // hold TOP partition
        bool cont = false;
        for(int pred : predecessors)
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
            // they are also marked processed
            int elSetId = -1;
            bool flag = true;
            for(int pred : predecessors) {
                int elSetIdPred = Partitions[pred][elIdx];
                if(elSetIdPred == -1) continue;
                else if(elSetId == -1) elSetId = elSetIdPred;
                else if(elSetId != elSetIdPred) flag = false;
            }

            if(flag) partition[elIdx] = elSetId;
            else {
                std::set<int> intersection, elClassPred;
                for(int i = 0; i < (int)IndexExp.size(); i++)
                    intersection.insert(i);
                
                for(int pred : predecessors) {
                    getClass(Partitions[pred], elIdx, elClassPred);
                    setIntersect<int>(intersection, elClassPred);
                }

                int newSetId = SetCnt++;
                for(int nel : intersection)
                    accessFlag[nel] = true, partition[nel] = newSetId;
            }
        }

        // now update `Parent` map
        for(auto &el : IndexExp) {
            char op = std::get<0>(el.first);
            if(op == '\0') continue;
            
            int leftSetID = partition[IndexExp[EXP(std::get<1>(el.first))]];
            int rightSetID = partition[IndexExp[EXP(std::get<2>(el.first))]];

            auto tup = std::make_tuple(op, leftSetID, rightSetID);
            // this assertion must be passed for the consistency of the algorithm
            assert(Parent.find(tup) == Parent.end() or 
                   Parent[tup] == partition[el.second]);

            Parent[tup] = partition[el.second];
        }
    }

    /**
     * @brief Main Herbrand analysis function.
     * 
     * @returns     Void
     * 
     * @see     Partitions, IndexExp, Parent
     **/
    void HerbrandAnalysis(Function &F) {
        PRINT_HEADER("Herbrand Equivalence Computation");
        errs() << "\n";

        // assign index to expressions arbitrarily
        assignIndex(F);

        // initialise partition vector with -1 for each program
        // points and each expression - this stands for TOP 
        // partition at each program point. Note that any element
        // of partition vector being -1 means that whole vector
        // holds -1 and represents TOP partition
        Partitions.assign(CFG.size(), 
                          std::vector<int>(IndexExp.size(), -1));

        // initialise starting partition for START node
        findInitialPartition(Partitions[0]);

        PRINT_HEADER("Initial Partition");
        printPartition(Partitions[0]);
        errs() << "\n\n\n";

        bool converged = false;
        int iterationCtr = 0;

        // repeat while convergence
        while(not converged) {
            PRINT_HEADER("Iteration " + std::to_string(++iterationCtr));
            converged = true;

            // for all program points (nodes in CFG) except
            // START apply transfer/confluence function as
            // applicable
            for(int i = 1; i < (int)CFG.size(); i++) {
                std::vector<int> oldPartition = Partitions[i];
                std::vector<int> &predecessors = CFG[i].predecessors;
                Instruction *inst = CFG[i].instPtr;

                errs() << '[' << i << "] : ";

                bool isConfluence = (CFG[i].NodeTy == CfgNodeTy::CONFLUENCE);
                if(CFG[i].NodeTy == CfgNodeTy::END and predecessors.size() > 1)
                    isConfluence = true;

                if(isConfluence) {
                    // if CFG node corresponds to a confluence point
                    if(CFG[i].NodeTy == CfgNodeTy::CONFLUENCE)
                        errs() << "Confluence Point => ";
                    else errs() << "END => ";
                    
                    errs() << "[Predecessors :";
                    for(auto el : predecessors) {
                        errs() << ' ' << el << '(' 
                               << CFG[el].instPtr->getParent()->getName() 
                               << ')';
                    }
                    errs() << "]\n\t";

                    confluenceFunction(i);
                } else {
                    // if CFG node corresponds to a transfer point

                    if(CFG[i].NodeTy == CfgNodeTy::TRANSFER) {
                        errs() << "Transfer Point => ["
                               << inst->getParent()->getName() << "] "
                               << (*inst) << ' ';
                    } else errs() << "END => ";

                    errs() << "[Predecessors : " << predecessors[0] << "]\n\t";

                    transferFunction(i);
                }

                printPartition(Partitions[i]);
                errs() << "\n\n";

                // update convergence flag
                if(not samePartition(oldPartition, Partitions[i]))
                    converged = false;
            }
            errs() << "\n\n";
        }
    }

    /**
     * @brief Body of the pass
     **/
    struct HerbrandPass : public FunctionPass {
        // LLVM uses ID’s address to identify a pass
        static char ID;

        // constructor - calls base class constructor
        HerbrandPass() : FunctionPass(ID) {}

        // the function pass
        bool runOnFunction(Function &F) override {
            // clear the contents of global variables
            Constants.clear(), Variables.clear();
            IndexExp.clear(), Partitions.clear();
            Parent.clear(), CFG.clear(), CfgIndex.clear();
            SetCnt = 0;

            // assign names to variables; create control flow graph
            assignNames(F), createCFG(F);
            printCode(F), printCFG();

            // perform Herbrand Analysis
            HerbrandAnalysis(F);

            // return false, because the pass is not making changes
            // in the input file
            return false;
        }
    };
}

// initialse static variable ID and register the pass
char HerbrandPass::HerbrandPass::ID = 0;
static RegisterPass<HerbrandPass::HerbrandPass> 
        Pass("HerbrandPass", "Herbrand equivalence analysis");
