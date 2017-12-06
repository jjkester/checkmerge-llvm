/**
 * @file SourceVariableMapper.h
 * @author Jan-Jelle Kester
 *
 * LLVM analysis pass that finds and stores a mapping between IR instructions and the original source variable.
 */
#ifndef CHECKMERGE_SOURCEVARIABLEMAPPER_H
#define CHECKMERGE_SOURCEVARIABLEMAPPER_H

#include <llvm/Pass.h>
#include <llvm/ADT/DenseMap.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/DebugLoc.h>
#include <llvm/IR/Value.h>

using namespace llvm;

// A source variable with the debug location of the instruction
typedef std::pair<const DILocalVariable *, const DebugLoc *> SourceVariable;
// A map between arbitrary values and a source variable
typedef DenseMap<const Value *, SourceVariable> SourceVariableMap;

struct SourceVariableMapper : public FunctionPass {

    static char ID;

    SourceVariableMap mapping;

    SourceVariableMapper() : FunctionPass(ID) {};

    // Implementation of the pass
    bool runOnFunction(Function &function) override;

    // Printer
    void print(raw_ostream &os, const Module *) const override;

    // Clean up
    void releaseMemory() override {
        this->mapping.clear();
    }

    // Define requirements and behavior
    void getAnalysisUsage(AnalysisUsage &usage) const override;

    /**
     * @return A mapping between arbitrary IR values and source variables.
     */
    SourceVariableMap getMapping() const;
};

#endif //CHECKMERGE_SOURCEVARIABLEMAPPER_H
