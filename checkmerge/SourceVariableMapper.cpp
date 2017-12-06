/**
 * @file SourceVariableMapper.cpp
 * @author Jan-Jelle Kester
 *
 * Definition of a LLVM analysis pass that finds and stores a mapping between IR instructions and the original source
 * variable.
 */
#include "SourceVariableMapper.h"

#include <llvm/IR/InstIterator.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/Support/FormatVariadic.h>

using namespace llvm;

bool SourceVariableMapper::runOnFunction(Function &function) {
    // Iterate over the instructions in a function
    for (Instruction &inst : instructions(function)) {
        // Check if the instruction is a debug instruction
        if (auto *dbgInst = dyn_cast<DbgInfoIntrinsic>(&inst)) {
            // Get variable information
            const DILocalVariable *sourceVar = dbgInst->getVariable();
            const Value *localVar = dbgInst->getVariableLocation();

            // Use instruction debug location since this is more accurate
            const DebugLoc &instDebugLoc = inst.getDebugLoc();

            // Save mapping if relevant
            if (dbgInst->isAddressOfVariable()) {
                mapping[localVar].first = sourceVar;
                mapping[localVar].second = &instDebugLoc;
            }
        }
    }
}

void SourceVariableMapper::print(raw_ostream &os, const Module *) const {
    os << formatv("Found {0} mappings", mapping.size()) << '\n';

    // Iterate over mappings
    for (const auto &variable : mapping) {
        const DILocalVariable *v = variable.second.first;
        const DebugLoc *l = variable.second.second;

        os << formatv("{0} ({1}) => {2} @ {3}:{4}", variable.first->getValueName()->first(), &variable.first, v->getName(), l->getLine(), l->getCol()) << '\n';
    }
}

SourceVariableMap SourceVariableMapper::getMapping() const {
    return this->mapping;
}

// Dependencies and behavior of this analysis
void SourceVariableMapper::getAnalysisUsage(AnalysisUsage &usage) const {
    usage.setPreservesAll();
}

char SourceVariableMapper::ID = 0;

static RegisterPass<SourceVariableMapper> SourceVariableMapperPass("checkmerge-vars", "CheckMerge Source Variable Mapping", false, true);
