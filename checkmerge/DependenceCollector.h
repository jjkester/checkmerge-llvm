/**
 * @file DependenceCollector.h
 * @author Jan-Jelle Kester
 *
 * Definition of a LLVM analysis pass that collects the results of the built-in memory dependence analysis and exposes
 * this with a simple interface.
 *
 * @note Code heavily inspired by and partially copied from the native llvm/lib/Analysis/MemDepPrinter.cpp file.
 */
#ifndef CHECKMERGE_DEPENDENCECOLLECTOR_H
#define CHECKMERGE_DEPENDENCECOLLECTOR_H

#include <llvm/Pass.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Metadata.h>
#include <llvm/ADT/SetVector.h>
#include <llvm/Analysis/MemoryDependenceAnalysis.h>

using namespace llvm;

/**
 * Types of dependencies that can be detected.
 */
enum DependencyType {
    Clobber = 0, /** Does unspeakable things to memory. */
    Def, /** Writes to memory. */
    NonFuncLocal, /** In other function, e.g., via a call. */
    Unknown /** All other cases. */
};

// The instruction that is dependent on including the type of dependency
typedef PointerIntPair<const Instruction *, 2, DependencyType> Dependency;
// The (optional) dependency (nullptr if  with an optional block (nullptr if the dependency is local to the block of
// the instruction).
typedef std::pair<Dependency, const BasicBlock *> DependencyPair;
// A set of dependencies
typedef SmallSetVector<DependencyPair, 4> DependencySet;
// The dependencies per instruction
typedef DenseMap<const Instruction *, DependencySet> DependencyMap;

/**
 * Analysis pass which, for every function, accumulates the memory dependencies of each instruction.
 */
struct DependenceCollector : public FunctionPass {

    // Metadata
    typedef SmallVector<std::pair<unsigned, MDNode *>, 4> MDVector;

    DependencyMap dependencies;
    Function *function;

    static char ID;

    DependenceCollector() : FunctionPass(ID) {
        this->function = nullptr;
    };

    // Pass implementation
    bool runOnFunction(Function &function) override;

    // Printer
    void print(raw_ostream &os, const Module *) const override;

    // Clean up memory
    void releaseMemory() override {
        this->dependencies.clear();
        this->function = nullptr;
    }

    // Define requirements and behavior
    void getAnalysisUsage(AnalysisUsage &usage) const override;

    /**
     * @return The resolved dependencies per instruction.
     */
    DependencyMap getDependencies() const;

private:

    /**
     * Builds a combination of the instruction and its dependency type from a dependency result.
     *
     * @param result The query result to define the dependency for.
     */
    static Dependency buildDependency(MemDepResult result);

    /**
     * Builds a pair of a dependency and the block it appears in.
     *
     * @param dependency The dependency to optionally pair with a block.
     * @param block The block to optionally pair with a dependency.
     */
    static DependencyPair buildDependencyPair(Dependency dependency, const BasicBlock * block);

    /**
     * String formats the given instruction with some debug information.
     *
     * @param inst The instruction to format.
     * @return A string representation of the instruction.
     */
    static std::string formatInst(const Instruction *inst);

    /**
     * String formats the debug location of the given instruction. May return the empty string if no location is
     * available.
     *
     * @param inst The instruction to format the location of.
     * @return A string representation of the original location of the instruction.
     */
    static std::string formatDebugLoc(const Instruction *inst);

    /**
     * String formats the type of a dependency.
     *
     * @param type The type to foramt.
     * @return A string representation of the type.
     */
    static std::string formatDependencyType(const DependencyType type);

    /**
     * Prints the dependencies of an instruction.
     *
     * @param os The output stream to print to.
     * @param inst The instruction to print the dependencies for.
     */
    void printInstDeps(raw_ostream &os, const Instruction *inst) const;
};

#endif //CHECKMERGE_DEPENDENCECOLLECTOR_H
