/** 
 * @file Program.h
 *  This file defines classes to capture a program, and
 *  also provides functionalities for working with it.
 **/

#ifndef PROGRAM_H
#define PROGRAM_H

#include<cassert>
#include<fstream>
#include<iostream>
#include<set>
#include<sstream>
#include<string>
#include<queue>
#include"MapVector.h"

// simple macro to print a header line to standard output
#ifndef PRINT_HEADER
#define PRINT_HEADER(str) std::cout << std::string(100, '=') << '\n' << (str) \
                                    << '\n' << std::string(100, '=') << '\n';
#endif

// forward declaration of `Program` class. This is required because 
// `CustomOStream` and `Program` classes are interdependent.
class Program;

/** 
 * @struct CustomOStream
 * @brief
 *  The << operator is overloaded for this structure, which then
 *  uses its data field `program`, to provide `std::cout` like 
 *  functionalities for `Program` class.
 * 
 * @see Program
 **/
struct CustomOStream {
    /** 
     * @brief
     *  The Program object over which this structure is parameterized
     **/
    Program &program;

    /**
     * @brief   Constructor for the structure
     * 
     * @param   program Program object over which the new object is to
     *                  be parameterised
     **/
    CustomOStream(Program &program) : program(program) {}
};

/**
 * @brief
 *  Defines structures and methods to capture a program,
 *  and operate with it.
 **/
class Program {
public:
    /**
     * @struct Program::ValueTy
     * 
     * @brief
     *  Used to capture variables and constants used in 
     *  the program.
     * 
     * @note
     *  The default value (when the variable does not 
     *  represent a valid constant/variable) for this class 
     *  is {false, -1}.
     * 
     * @see Program::Variables, Program::Constants
     **/
    struct ValueTy {
        /**
         * @brief
         *  Whether the value represented is a constant
         *  or a variable.
         **/
        bool isConst;

        /** 
         * @brief
         *  The corresponding index of the constant or 
         *  variable as given by `Program::Variables` and 
         *  `Program::Constants`.
         * 
         * @see Program::Variables, Program::Constants
         **/
        int index;
    };

    /**
     * @struct Program::ExpressionTy
     * 
     * @brief
     *  Captures expressions.
     * 
     * @details
     *  An expression can either be a constant or a variable
     *  or a length two expression.
     * 
     * @see Program::ValueTy
     **/
    struct ExpressionTy {
        /**
         * @brief
         *  Operator in the expression. If the expression represents
         *  non-deterministic assignment then this field holds `#`.
         *  Otherwise, if it is not a two length expression then this 
         *  field defaults to `\0` for consistency. For these two 
         *  exceptional cases the other two fields are irrelavent and 
         *  holds their default values (which is {false, -1}).
         **/
        char op;

        /**
         * @brief
         *  If the expression is of length two it holds the left
         *  operand, else the constant/variable corresponding to
         *  the expression. 
         **/
        ValueTy leftOp;
        
        /** 
         * @brief
         *  If the expression is of length two it holds the right
         *  operand, else it defaults to {false, -1} for consistency.
         **/
        ValueTy rightOp;
    };

    /**
     * @struct Program::InstructionTy
     * 
     * @brief
     *  Captures instructions in the program
     * 
     * @details
     *  An instruction is combination of an lvalue and a 
     *  rvalue, where lvalue is a variable and rvalue is 
     *  an expression.
     * 
     * @see Program::ExpressionTy, Program::ValueTy
     **/
    struct InstructionTy {
        /**
         * @brief
         *  Lvalue of the instruction, which is a variable.
         **/
        ValueTy lValue;

        /**
         * @brief
         *  Rvalue of the instruction, which is an expression.
         **/
        ExpressionTy rValue;

        /**
         * @brief
         *  Boolean for whether this instruction is reachable
         *  from the beginning of the program.
         **/
        bool reachable;

        /**
         * @brief
         *  Index of the node representing this instruction in 
         *  the control flow graph corresponding to the program
         *  (as given by `Program::CFG` data member).
         * 
         * @see Program::CFG
         **/
        int cfgIndex;

        /**
         * @brief
         *  Index of the predecessors of this instructions as given
         *  by `Program::Instructions` data member.
         * 
         * @see Program::Instructions
         **/
        std::set<int> predecessors;
    };

    /**
     * @struct Program::CfgNodeTy
     * 
     * @brief
     *  Represents a node in the control flow graph of the program.
     **/
    struct CfgNodeTy {
        /**
         * @brief
         *  Index of predecessor control flow nodes as given
         *  by `Program::CFG`.
         * 
         * @see Program::CFG
         **/
        std::vector<int> predecessors;

        /**
         * @brief
         *  Index of the instruction which this node represents
         *  as given by `Program::Instructions`, if the node does
         *  not corresponds to an actual instruction then this 
         *  field defaults to -1 (such nodes can represent START
         *  or END or confluence point).
         * 
         * @see Program::Instructions
         **/
        int instructionIndex;
    };

    /**
     * @brief Variables used in the program.
     * 
     * @details
     *  It is a `MapVector` object which maps variable
     *  names to unique integers, so that we finally
     *  have to work only with integers instead of
     *  working with strings. Also `MapVector` maps
     *  unique strings to unique integers, which also
     *  helps to identify same variable used at different
     *  places.
     * 
     * @see MapVector
     **/
    MapVector<std::string> Variables;

    /**
     * @brief Constants used in the program.
     * 
     * @details
     *  It is a `MapVector` object which maps unique integers
     *  to new unique integers. The new mapped integers are
     *  used to while working with the program.
     * 
     * @see MapVector
     **/
    MapVector<int> Constants;

    /**
     * @brief Instructions in the program.
     * 
     * @details
     *  The first and last element of this vector are
     *  dummy which stands for START and END instructions.
     *  They facilitates branching and confluence at the 
     *  start and the end of the program respectively.
     * 
     * @see Program::InstructionTy
     **/
    std::vector<InstructionTy> Instructions;

    /**
     * @brief Control flow graph corresponding to the program.
     * 
     * @details
     *  It contains nodes corresponding to the START, END,
     *  reachable instructions and confluence points.
     * 
     * @see Program::CfgNodeTy
     **/
    std::vector<CfgNodeTy> CFG;

    /**
     * @brief
     *  To provide `std::cout` like functionality for `Program` class.
     * 
     * @note
     *  Any use of `cout` refers to `Program::cout`, unless it 
     *  is `std::cout`.
     * 
     * @see CustomOStream
     **/
    CustomOStream cout;

    /**
     * @brief Constructor for Program class
     **/
    Program() : cout(*this) {}

    /**
     * @brief
     *  Parses and captures a program.
     * 
     * @param   fname   Filename which contains the program text
     * @return  None
     * 
     * @see Constants, Instructions, Variables
     **/
    void parse(std::string fname);

    /**
     * @brief
     *  Prints the program to standard output in a readable format.
     * 
     * @return  None
     **/
    void print();

    /**
     * @brief
     *  Creates control flow graph corresponding to the program.
     * 
     * @return  None
     * 
     * @see CFG
     **/
    void createCFG();

    /**
     * @brief
     *  Prints the control flow graph corresponding to the program
     *  in a readable format.
     * 
     * @return  None
     **/
    void printCFG();
};

/** 
 * @brief
 *  Overloading < operator for some strict ordering on 
 *  Program::ValueTy. It is required for creating 
 *  `std::set` and `std::map` objects.
 **/
bool operator<(Program::ValueTy const &first, 
               Program::ValueTy const &second) {
    if(first.isConst == second.isConst)
        return first.index < second.index;
    return first.isConst;
}

/** 
 * @brief
 *  Overloading < operator for some strict ordering 
 *  on Program::ExpressionTy. It is required for 
 *  creating `std::set` and `std::map` objects.
 **/
bool operator<(Program::ExpressionTy const &first, 
               Program::ExpressionTy const &second) {
    if(first.op != second.op) return first.op < second.op;
    if(first.leftOp < second.leftOp) return true;
    if(second.leftOp < first.leftOp) return false;
    return first.rightOp < second.rightOp;
}

CustomOStream& operator<<(CustomOStream &os, char ch) {
    std::cout << ch;
    return os;
}

CustomOStream& operator<<(CustomOStream &os, int i) {
    std::cout << i;
    return os;
}

CustomOStream& operator<<(CustomOStream &os, std::string s) {
    std::cout << s;
    return os;
}

CustomOStream& operator<<(CustomOStream &os, Program::ValueTy const &v) {
    if(v.isConst) std::cout << os.program.Constants[v.index];
    else std::cout << os.program.Variables[v.index];
    return os;
}

CustomOStream& operator<<(CustomOStream &os, Program::ExpressionTy const &e) {
    if(e.op == '\0') return os << e.leftOp;
    if(e.op == '#') return os << '*';
    return os << e.leftOp << ' ' << e.op << ' ' << e.rightOp;
}

CustomOStream& operator<<(CustomOStream &os, Program::InstructionTy const &i) {
    return os << i.lValue << " = " << i.rValue;
}

/**
 * @brief Parses and captures a program.
 * 
 * @param   fname   Filename which contains the program text
 * @return  None  
 * 
 * @see Constants, Instructions, Variables
 **/
void Program::parse(std::string fname) {
    std::ifstream fin(fname);
    assert(fin && "Error opening file");

    // keeps count of the current instruction number. It is 
    // initialised 1 because 0th instruction is the dummy START
    // instruction
    int instCnt = 1;

    // holds a program line
    std::string buf;

    // holds jump labels for instructions corresponding to their indexes.
    // If `jumps` vector is empty for an instruction, it means a default
    // jump to the instruction corresponding to the next index
    std::vector<std::vector<std::string>> jumps;

    // holds the instruction indexes to which the labels refer to
    std::map<std::string, int> labels;

    // updating `jumps` and `Program::Instructions` for START instruction
    InstructionTy dummy = {{false, -1}, {'\0', {false, -1}, {false, -1}}, false, {}};
    Instructions.emplace_back(dummy);
    jumps.emplace_back(std::vector<std::string>());

    // read the program, processing each instruction type and checking
    // the validity. The jumps can not be resolved at this stage (before
    // the whole program is processed), as labels can be used before
    // definitions
    while(getline(fin, buf)) {
        // ignores empty lines in the program text
        if(buf.empty()) continue;

        // buffer holding current instruction
        std::stringstream ss(buf);
        
        // holds the tokens in the instruction while they are processed
        // in succession 
        std::string in;

        // read the first token in the instruction
        ss >> in;

        if(in == "GOTO") {
            while(ss >> in)
                jumps[instCnt - 1].push_back(in);
        } else if(in == "LABEL") {
            // current instruction defines a label
            while(ss >> in) {
                assert(labels.find(in) == labels.end() && "Duplicate label found");
                labels[in] = instCnt;
            }
        } else {
            // current instruction is an assignment instruction

            int constVal;
            ValueTy lValue, leftOp, rightOp;
            char op = '\0';
            
            try { 
                // ideally lvalue should be a variable (a string that does
                // not starts with a digit). So this must throw an exception 
                // and we should reach catch block.
                constVal = std::stoi(in.c_str()); 

                assert(false && "LValue is not a variable");
            } catch(std::invalid_argument) {
                // storing lvalue of current instruction       
                lValue = {false, Variables.insert(in).first};

                // read and ignore the `=` in the instruction buffer
                in.clear(), ss >> in;
                assert(in == "=" && "No RValue specified");

                // read the first operand from the instruction buffer
                in.clear(), ss >> in;
                assert(!in.empty() && "No RValue specified");

                // store left operand of rvalue of current instruction
                try {
                    constVal = std::stoi(in.c_str());
                    leftOp = {true, Constants.insert(constVal).first};
                } catch(std::invalid_argument) {
                    if(in == "*") op = '#', leftOp = {false, -1};
                    else leftOp = {false, Variables.insert(in).first};
                }

                // read the next token in the instruction buffer (which should
                // be the operator if any)
                in.clear(), ss >> in;

                // if the current instruction is non-deterministic assignment
                // then it can not have a second operand. Note that if the 
                // rvalue is non-deterministic only then `op` has value `#`.
                assert((op != '#' || in.empty()) && "Invalid instruction");

                if(!in.empty()) {
                    // if the current instruction has rvalue, which is two 
                    // length expression

                    // determine the operator and check if it is valid.
                    // If the operator is invalid then op will have value 
                    // '\0', by which it was initialised
                    if(in.length() == 1) {
                        switch(in[0]) {
                            case '+':
                            case '-':
                            case '*':
                            case '/': op = in[0];
                        }
                    }
                    assert(op != '\0' && "Invalid operand");

                    // read the second operand from the instruction buffer
                    in.clear(), ss >> in;
                    assert(!in.empty() && "Second operand not specified");

                    // store right operand of rvalue of current instruction
                    try {
                        constVal = std::stoi(in.c_str());
                        rightOp = {true, Constants.insert(constVal).first};
                    } catch(std::invalid_argument) {
                        rightOp = {false, Variables.insert(in).first};
                    }
                } else {
                    // if rvalue is not two length expression, a default value
                    // value for right operand for consistency
                    rightOp = {false, -1};
                }
            }

            // push the current instruction in `Instructions` vector
            // at index `instCnt`. For now `reachable` and `cfgIndex`
            // fields are set to default value of false and -1. These
            // would be set to actual values later.
            Instructions.push_back({lValue, {op, leftOp, rightOp}, false, -1, {}});
            jumps.emplace_back(std::vector<std::string>());
            instCnt++;
        }
    }

    // updating `jumps` and `Program::Instructions` for END instruction
    Instructions.push_back(dummy);
    jumps.emplace_back(std::vector<std::string>());
    instCnt++;

    // now process the jumps as given by the labels. Reachability
    // is determined by performing breadth first search (BFS) from
    // index 0 in `Instructions` (the START instruction). 
    // Instructions for which `jumps` list is empty has by default 
    // jump to the instruction corresponding to the next index.
    std::queue<int> q;
    q.push(0), Instructions[0].reachable = true;

    while(not q.empty()) {
        int cur = q.front();
        q.pop();

        if(not jumps[cur].empty()) {
            // process the jumps
            for(auto el : jumps[cur]) {
                auto it = labels.find(el);
                assert(it != labels.end() && "Undefined label found");

                // if the label points to index `instCnt`, it means
                // the current instruction being processed is the dummy
                // END instruction and this has to be ignored
                if(it->second == instCnt) continue;

                Instructions[it->second].predecessors.insert(cur);
                if(not Instructions[it->second].reachable) {
                    Instructions[it->second].reachable = true;
                    q.push(it->second);
                }
            }
        } else {
            // if the index of the next instruction is `instCnt`, it 
            // means the current instruction being processed is the 
            // dummy END instruction and this has to be ignored
            if(cur + 1 == instCnt) continue;

            // the successor of current instruction is the next 
            // instruction index
            Instructions[cur + 1].predecessors.insert(cur);            
            if(not Instructions[cur + 1].reachable) {
                Instructions[cur + 1].reachable = true;
                q.push(cur + 1);
            }
        }
    }

    fin.close();
}

/**
 * @brief
 *  Prints the program to standard output in a readable format.
 * 
 * @return  None
 **/
void Program::print() {
    // print all the variables
    PRINT_HEADER("Variables");
    for(auto &el : Variables)
        cout << el << ", ";
    cout << "\n\n";

    // print all the constants
    PRINT_HEADER("Constants");
    for(auto &el : Constants)
        cout << el << ", ";
    cout << "\n\n";

    // now print all the instructions, if they are
    // reachable print their predecessors else mark
    // them as unreachable
    PRINT_HEADER("Input Program");
    
    int sz = Instructions.size();
    for(int i = 0; i < sz; i++) {
        cout << '[' << i << "] : ";
        
        if(i == 0) cout << "START";
        else if(i == sz - 1) cout << "END";
        else cout << Instructions[i];
        
        if(Instructions[i].reachable) {
            cout << "\t[ Predecessor Instructions : ";
            for(auto el : Instructions[i].predecessors)
                cout << el << " ";
            cout << "]\n";
        } else cout << "\t[ Unreachable ]\n";
    }

    cout << "\n\n";
}

/**
 * @brief
 *  Creates control flow graph corresponding to the program.
 * 
 * @return  None
 * 
 * @see CFG
 **/
void Program::createCFG() {
    // holds the size of the control flow graph
    int cfgSize = 0;

    // first set `cfgIndex` for each instruction
    for(auto &el : Instructions) {
        // there are no node in CFG corresponding to unreachable
        // instructions, so we skip them
        if(not el.reachable) continue;

        if(el.predecessors.size() > 1) {
            // if the current instruction is a confluence point.
            // Note that this condition is not `!= 1`, because it
            // must also be false for the START instruction (for which 
            // `== 0` is true and hence `!= 1` would then be true)

            // there would be a node for the instruction as well as 
            // for the confluence point. The confluence point gets 
            // index `cfgSize` and the  instruction gets index 
            // `cfgSize + 1`. Only 'cfgSize + 1' is stored, and
            // the index of confluence point in CFG can be inferred 
            // with its help
            el.cfgIndex = cfgSize + 1;

            // update `cfgSize`
            cfgSize += 2;
        } else {
            // there would only be node for the instruction which
            // gets index `cfgSize` and `cfgSize` is incremented
            el.cfgIndex = cfgSize++;
        }
    }

    // now create control flow graph, using assigned `cfgIndex`
    // for each instructions
    CFG.resize(cfgSize);

    for(int i = 0; i < Instructions.size(); i++) {
        InstructionTy &I = Instructions[i];

        // ignore unreachable instructions
        if(not I.reachable) continue;

        int idx = I.cfgIndex;
        int sz = I.predecessors.size();

        if(sz > 1) {
            // if the instruction has more than one predecessors

            // node for the confluence point
            int confIdx = idx - 1;
            CFG[confIdx].instructionIndex = -1;
            for(auto el : I.predecessors) {
                int predIdx = Instructions[el].cfgIndex;
                CFG[confIdx].predecessors.push_back(predIdx);
            }
            
            // node for the instruction
            CFG[idx].predecessors.push_back(confIdx);
            CFG[idx].instructionIndex = i;
        } else {
            // if the instruction is not the START instruction
            // update the predecessor for the node corresponding 
            // to the instruction
            if(sz != 0) {
                int predIdx = Instructions[*(I.predecessors.begin())].cfgIndex;
                CFG[idx].predecessors.push_back(predIdx);
            }
            CFG[idx].instructionIndex = i;
        }
    }
}

/**
 * @brief
 *  Prints the control flow graph corresponding to the program
 *  in a readable format.
 * 
 * @return  None
 **/
void Program::printCFG() {
    PRINT_HEADER("Control Flow Graph");

    int cfgSize = CFG.size();
    for(int i = 0; i < cfgSize; i++) {
        cout << '[' << i << "] : ";

        int sz = CFG[i].predecessors.size();
        int idx = CFG[i].instructionIndex;

        if(sz == 0) cout << "START\n";
        else if(sz == 1) {
            // for normal control flow graph nodes, print the 
            // instruction index (as given by `Instructions`),
            // the instruction itself and then the index of 
            // the predecssor control flow graph node
            if(idx == Instructions.size() - 1) cout << "END";
            else cout << "Transfer Point => (" << idx << ") " 
                      << Instructions[idx];
                 
            cout << " [ Predecessor CFG Node : " 
                 << CFG[i].predecessors[0] << " ]\n";
        } else {
            // for confluence points print the indexes of the 
            // predecessor control flow graph nodes
            cout << "Confluence Point => [ Predecessor CFG Nodes : ";
            for(auto el : CFG[i].predecessors)
                cout << el << " ";
            cout << "]\n";
        }
    }
    
    cout << "\n\n";
}

#endif