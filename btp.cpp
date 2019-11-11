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

template <typename T>
void setIntersect(set<T> &x, set<T> &y) {
    for(auto it = x.begin(); it != x.end(); ) {
        auto temp = it++;
        if(y.find(*temp) == y.end())
            x.erase(temp);
    }
}

template <typename T>
void setUnion(set<T> &x, set<T> &y) {
    for(auto el : y)
        x.insert(y);
}

namespace {
    struct IDstruct {
        string ftype;
        int valueNum;
        int parentCnt;
        IDstruct *left;
        IDstruct *right;
        static int valNumCtr;

        IDstruct() : ftype(""), valueNum(valNumCtr++), parentCnt(0), left(nullptr), right(nullptr) {}
        IDstruct(string op, IDstruct *left, IDstruct *right) {
            this->ftype = op;
            this->valueNum = -1;
            this->parentCnt = 0;
            this->left = left;
            this->right = right;
        }
    };
    int IDstruct::valNumCtr = 0;

    typedef tuple<string, Value *, Value *> tupleA;
    typedef tuple<string, IDstruct *, IDstruct *> tupleB;

    set<Value *> Constants, Variables, CuV;
    set<string> Ops({"+"});
    map<Value *, int> mpCV;
    map<tupleA, int> mpExp;
    int numClasses;
    map<Instruction *, vector<IDstruct *>> Partitions;
    map<tupleB, IDstruct *> Parent;

    string opName(string opCodeName) {
        if(opCodeName == "add") return "+";
        if(opCodeName == "sub") return "-";
        if(opCodeName == "mul") return "*";
        if(opCodeName == "sdiv") return "/";
        if(opCodeName == "udiv") return "/";
        return "";
    }

    void assignIndex(Function &F) {
        int ctr = 0;
        int cnt = 1;
        for(Instruction &I : instructions(&F)) {
            if(!I.getType()->isVoidTy()) {
                Variables.insert(&I);
                I.setName("T");
            }
            if(isa<AllocaInst>(&I)) continue;

            for(int i = 0; i < (int)I.getNumOperands(); i++) {
                Value *value = I.getOperand(i);
                if(dyn_cast<Constant>(value)) {
                    Constants.insert(value);
                } else {
                    Variables.insert(value);
                    if(not value->hasName()) {
                        value->setName("T");
                    }
                }
            }
        }
        CuV = Constants, CuV.insert(Variables.begin(), Variables.end());

        for(auto el : CuV) mpCV[el] = ctr++;
        for(auto op : Ops)
            for(auto x : CuV)
                for(auto y : CuV)
                    mpExp[{op, x, y}] = ctr++;
        numClasses = ctr;
    }

    void findInitialPartition(vector<IDstruct *> &v) {
        v.resize(numClasses);
        for(auto el : CuV) {
            IDstruct *ptr = new IDstruct;
            v[mpCV[el]] = ptr;
            ptr->parentCnt += 1;
        }
        for(auto op : Ops)
            for(auto x : CuV)
                for(auto y : CuV) {
                    IDstruct *left = v[mpCV[x]];
                    IDstruct *right = v[mpCV[y]];

                    IDstruct *ptr = new IDstruct(op, left, right);
                    Parent[{op, left, right}] = ptr;
                    left->parentCnt += 1;
                    right->parentCnt += 1;

                    v[mpExp[{op, x, y}]] = ptr;
                    ptr->parentCnt += 1;
                }
    }

    void getClass(int index, vector<IDstruct *> const &partition, set<Value *> &vV, set<tupleA> &vTup) {
        IDstruct *ptr = partition[index];
        vV.clear(), vTup.clear();
        
        if(ptr == nullptr) return;

        for(auto &el : mpCV)
            if(partition[el.second] == ptr)
                vV.insert(el.first);
        for(auto &el : mpExp)
            if(partition[el.second] == ptr)
                vTup.insert(el.first);
    }

    bool samePartition(vector<IDstruct *> const &first, vector<IDstruct *> const &second) {
        set<Value *> vVfirst, vVsecond;
        set<tupleA> vTupfirst, vTupsecond;
        for(auto el : mpCV) {
            getClass(el.second, first, vVfirst, vTupfirst);
            getClass(el.second, second, vVsecond, vTupsecond);
            if(vVfirst != vVsecond || vTupfirst != vTupsecond)
                return false;
        }
        for(auto el : mpExp) {
            getClass(el.second, first, vVfirst, vTupfirst);
            getClass(el.second, second, vVsecond, vTupsecond);
            if(vVfirst != vVsecond || vTupfirst != vTupsecond)
                return false;
        }
        return true;
    }

    void decreaseParentCnt(IDstruct *&ptr) {
        if(ptr == nullptr) return;

        ptr->parentCnt -= 1;
        if(ptr->parentCnt > 0) return;

        // here we can also use `ptr->ftype != ""` or `ptr->left != nullptr` or `ptr->right != nullptr`
        if(ptr->left != nullptr && ptr->right != nullptr) {
            auto it = Parent.find({ptr->ftype, ptr->left, ptr->right});
            Parent.erase(it);
            decreaseParentCnt(ptr->left);
            decreaseParentCnt(ptr->right);
        }
        delete(ptr);
        ptr = nullptr;
    }

    void increaseParentCnt(IDstruct *&ptr) {
        if(ptr == nullptr) return;
        ptr->parentCnt += 1;
    }

    void copyPartition(vector<IDstruct *> &curPart, vector<IDstruct *> const &prevPart) {
        for(auto &el : curPart)
            decreaseParentCnt(el);
        curPart = prevPart;
        for(auto &el : curPart)
            increaseParentCnt(el);
    }

    IDstruct* findIDstrct(vector<IDstruct *> const &curPart, string op, Value *first, Value *second) {
        IDstruct *ptrf = curPart[mpCV[first]];
        IDstruct *ptrs = curPart[mpCV[second]];

        auto it = Parent.find({op, ptrf, ptrs});
        if(it != Parent.end()) return (it->second);

        IDstruct *ptr = new IDstruct(op, ptrf, ptrs);
        Parent[{op, ptrf, ptrs}] = ptr;
        increaseParentCnt(ptrf), increaseParentCnt(ptrs);
        
        return ptr;
    }

    void printCV(Value const *value) {
        if(dyn_cast<Constant>(value))
            errs() << dyn_cast<ConstantInt>(value)->getValue().toString(10, true);
        else errs() << value->getName();
    }

    void printExp(tupleA const &tup) {
        Value *x = get<1>(tup), *y = get<2>(tup);
        string op = get<0>(tup);
        printCV(x);
        errs() << " " << op << " ";
        printCV(y);
    }

    void AssignStatement(vector<IDstruct *> &curPart, vector<IDstruct *> const &prevPart, Instruction &I) {
        copyPartition(curPart, prevPart);
        
        Value *changed = nullptr;

        if(isa<LoadInst>(I)) {
            Value *first = I.getOperand(0);
            IDstruct *ptrf = curPart[mpCV[first]];
            IDstruct *ptrI = curPart[mpCV[&I]];

            decreaseParentCnt(ptrI);
            curPart[mpCV[&I]] = ptrf;
            increaseParentCnt(ptrf);

            changed = &I;
        } else if(isa<StoreInst>(I)) {
            Value *first = I.getOperand(0);
            Value *second = I.getOperand(1);
            IDstruct *ptrf = curPart[mpCV[first]];
            IDstruct *ptrs = curPart[mpCV[second]];

            decreaseParentCnt(ptrs);
            curPart[mpCV[second]] = ptrf;
            increaseParentCnt(ptrf);

            changed = second;
        } else if(I.isBinaryOp()) {
            Value *first = I.getOperand(0);
            Value *second = I.getOperand(1);
            string op = opName(I.getOpcodeName());

            IDstruct *ptrI = curPart[mpCV[&I]];
            IDstruct *ptrn = findIDstrct(curPart, op, first, second);

            decreaseParentCnt(ptrI);
            curPart[mpCV[&I]] = ptrn;
            increaseParentCnt(ptrn);

            changed = &I;
        }

        if(changed == nullptr) return;

        for(auto op : Ops)
            for(auto x : CuV) {
                IDstruct *ptr;
                
                decreaseParentCnt(curPart[mpExp[{op, x, changed}]]);
                ptr = findIDstrct(curPart, op, x, changed);
                curPart[mpExp[{op, x, changed}]] = ptr;
                increaseParentCnt(ptr);

                decreaseParentCnt(curPart[mpExp[{op, changed, x}]]);
                ptr = findIDstrct(curPart, op, changed, x);
                curPart[mpExp[{op, changed, x}]] = ptr;
                increaseParentCnt(ptr);
            }

    }

    void findPredecessors(Instruction &I, vector<Instruction *> &v) {
        v.clear();
        for(auto it = pred_begin(I.getParent()); it != pred_end(I.getParent()); it++)
            v.push_back(&(*it)->back());
    }

    void confluence(vector<IDstruct *> &partition, Instruction &I) {
        vector<IDstruct *> tempPartition(numClasses, nullptr);
        vector<Instruction *> predInsts;
        findPredecessors(I, predInsts);

        vector<bool> accessFlag(CuV.size(), false);
        for(auto el : mpCV) {
            if(not accessFlag[el.second]) {
                accessFlag[el.second] = true;
                bool flag = true;

                for(int i = 1; i < (int)predInsts.size() && flag; i++)
                    if(Partitions[predInsts[0]][el.second] != Partitions[predInsts[i]][el.second])
                        flag = false;

                if(flag) tempPartition[el.second] = Partitions[predInsts[0]][el.second];
                else {
                    set<Value *> vV, intersection(CuV);
                    set<tupleA> vTup;
                    for(auto inst : predInsts) {
                        getClass(el.second, Partitions[inst], vV, vTup);
                        setIntersect<Value *>(intersection, vV);
                    }

                    IDstruct *ID = new IDstruct();
                    for(auto nel : intersection) {
                        accessFlag[mpCV[nel]] = true;
                        tempPartition[mpCV[nel]] = ID;
                    }
                }
            }
        }

        for(auto el : mpExp) {
            Value *left = get<1>(el.first), *right = get<2>(el.first);
            string op = opName(get<0>(el.first));
            tempPartition[el.second] = findIDstrct(tempPartition, op, left, right);
        }

        AssignStatement(partition, tempPartition, I);
    }

    void printPartition(vector<IDstruct *> const &partition) {
        vector<bool> done(numClasses, false);
        set<Value *> vV;
        set<tupleA> vTup;

        for(auto el : mpCV) {
            if(done[el.second]) continue;

            getClass(el.second, partition, vV, vTup);
            errs() << "{";
            
            for(auto it = vV.begin(); it != vV.end();) {
                printCV(*it);
                done[mpCV[*it]] = true;
                if(++it != vV.end()) errs() << ", ";
            }

            if(!vV.empty() && !vTup.empty()) errs() << ", ";

            for(auto it = vTup.begin(); it != vTup.end();) {
                printExp(*it);    
                done[mpExp[*it]] = true;
                if(++it != vTup.end()) errs() << ", ";
            }

            errs() << "}, ";
        }

        for(auto el : mpExp) {
            if(done[el.second]) continue;
            
            getClass(el.second, partition, vV, vTup);
            errs() << "{";
            
            for(auto it = vV.begin(); it != vV.end();) {
                printCV(*it);
                done[mpCV[*it]] = true;
                if(++it != vV.end()) errs() << ", ";
            }

            if(!vV.empty() && !vTup.empty()) errs() << ", ";

            for(auto it = vTup.begin(); it != vTup.end();) {
                printExp(*it);
                done[mpExp[*it]] = true;
                if(++it != vTup.end()) errs() << ", ";
            }
            errs() << "}, ";
        }
    }

    struct btp : public FunctionPass {
        static char ID;
        static int BBcnt;
        btp() : FunctionPass(ID) {}

        bool runOnFunction(Function &F) override {
            assignIndex(F);
            vector<IDstruct *> initialPartition;
            findInitialPartition(initialPartition);
            errs() << string(150, '=') << "\nInitial Partition " << "\n" << string(150, '=') << "\n";
            printPartition(initialPartition);
            errs() << "\n\n";

            for(Instruction &I : instructions(F)) findInitialPartition(Partitions[&I]);

            bool converged = false;
            int iterationCtr = 0;
            while(not converged) {
                errs() << string(75, '=') << "\nIteration " << ++iterationCtr << "\n" << string(75, '=') << "\n";
                converged = true;
                
                Instruction *prevInst = nullptr;
                for(Instruction &I : instructions(F)) {
                    errs() << I << "\n";
                    // if(Partitions.find(&I) == Partitions.end()) Partitions[&I] = vector<IDstruct *>(numClasses, nullptr);

                    vector<IDstruct *> oldPartition = Partitions[&I];
                    if(prevInst == nullptr)
                        AssignStatement(Partitions[&I], initialPartition, I);
                    else if(&I.getParent()->front() == &I)
                        confluence(Partitions[&I], I);
                    else AssignStatement(Partitions[&I], Partitions[prevInst], I);

                    prevInst = &I;
                    if(not samePartition(oldPartition, Partitions[&I])) converged = false;
                    printPartition(Partitions[&I]);
                    errs() << "\n\n\n";
                }
            }
            return false;
        }
    };
}

char btp::ID = 0;
static RegisterPass<btp> X("btp", "Herbrand equivalence analysis");
