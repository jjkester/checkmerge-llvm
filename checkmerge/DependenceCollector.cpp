/**
 * @file DependenceCollector.cpp
 * @author Jan-Jelle Kester
 *
 * LLVM analysis pass that collects the results of the built-in memory dependence analysis and exposes this with a
 * simple interface.
 *
 * @note Code heavily inspired by and partially copied from the native llvm/lib/Analysis/MemDepPrinter.cpp file.
 */
#include "DependenceCollector.h"

#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/Support/FormatVariadic.h>
#include "SourceVariableMapper.h"

using namespace llvm;

// Dependencies and behavior of this analysis
void DependenceCollector::getAnalysisUsage(AnalysisUsage &usage) const {
    usage.setPreservesAll();
    usage.addRequired<MemoryDependenceWrapperPass>();
//    usage.addRequired<SourceVariableMapper>();
}

Dependency DependenceCollector::buildDependency(MemDepResult result) {
    DependencyType type = DependencyType::Unknown;

    if (result.isClobber()) {
        type = DependencyType::Clobber;
    }
    if (result.isDef()) {
        type = DependencyType::Def;
    }
    if (result.isNonFuncLocal()) {
        type = DependencyType::NonFuncLocal;
    }

    return {result.getInst(), type};
}

DependencyPair DependenceCollector::buildDependencyPair(Dependency dependency, const BasicBlock * block) {
    return static_cast<DependencyPair>(std::make_pair(dependency, block));
}

std::string DependenceCollector::formatInst(const Instruction *inst) {
    // Initialize data variables
    std::string locStr, idStr;

    locStr = formatDebugLoc(inst);

    const StringRef instName = inst->getName();

    idStr = formatv("[{0}] {1}", instName, inst->getOpcodeName());

    if (locStr.empty()) {
        return formatv("{0} ({1})", idStr, inst);
    } else {
        return formatv("{0} ({1}) @ {2}", idStr, inst, locStr);
    }
}

 std::string DependenceCollector::formatDebugLoc(const Instruction *inst) {
    // Collect metadata
    MDVector metadata;
    inst->getAllMetadata(metadata);

    for (auto &metapair : metadata) {
        if (MDNode *node = metapair.second) {
            // Fetch location
            if (auto *location = dyn_cast<DILocation>(node)) {
                return formatv("{0}:{1}:{2}", location->getFilename(), location->getLine(), location->getColumn());
            }
        }
    }

    return "";
}

std::string DependenceCollector::formatDependencyType(const DependencyType type) {
    switch (type) {
        case DependencyType::NonFuncLocal: return "non-local";
        case DependencyType::Clobber: return "clobber";
        case DependencyType::Def: return "def";
        default: return "unknown";
    }
}

/**
 * Iterates over the instructions in each function and queries the memory dependence analysis to find the memory
 * dependencies of each memory instruction.
 *
 * @param function The function to analyze.
 */
bool DependenceCollector::runOnFunction(Function &function) {
    // Set function pointer
    this->function = &function;

    // Get memory dependence results
    MemoryDependenceResults &results = getAnalysis<MemoryDependenceWrapperPass>().getMemDep();

    // Iterate over instructions in function
    for (auto &I : instructions(function)) {
        Instruction *inst = &I;

        // Continue if this instruction does not do anything with memory
        if (!inst->mayReadOrWriteMemory()) {
            continue;
        }

        // Get dependence result for the instruction
        MemDepResult result = results.getDependency(inst);

        if (!result.isNonLocal()) {
            // If the dependency is local
            Dependency dependency = buildDependency(result);
            dependencies[inst].insert(buildDependencyPair(dependency, static_cast<BasicBlock *>(nullptr)));
        } else if (auto callSite = CallSite(inst)) {
            // If the dependency is a call or invoke (so not local)
            const MemoryDependenceResults::NonLocalDepInfo &info = results.getNonLocalCallDependency(callSite);

            // For all blocks calling to this instruction, save the dependency
            for (const NonLocalDepEntry &entry : info) {
                const MemDepResult &depResult = entry.getResult();
                Dependency dependency = buildDependency(depResult);
                dependencies[inst].insert(buildDependencyPair(dependency, entry.getBB()));
            }
        } else {
            // If the dependency is load, store or argument (or other)
            SmallVector<NonLocalDepResult, 4> depResults;
            results.getNonLocalPointerDependency(inst, depResults);

            // For all blocks pointing to this instruction, save the dependency
            for (const NonLocalDepResult &nonLocalDepResult : depResults) {
                const MemDepResult &depResult = nonLocalDepResult.getResult();
                Dependency dependency = buildDependency(depResult);
                dependencies[inst].insert(buildDependencyPair(dependency, nonLocalDepResult.getBB()));
            }
        }
    }

    // We do not modify anything, so return false
    return false;
}

void DependenceCollector::print(raw_ostream &os, const Module *) const {
    // Quit if we don't know the function
    if (this->function == nullptr) {
        return;
    }

    // Get variable mapping
//    SourceVariableMap mapping = getAnalysis<SourceVariableMapper>().getMapping();

    // Print function name
    os << formatv("Function [{0}]", function->getName()) << '\n';

    for (const auto &block : *function) {
        // Print basic block
        os << formatv("  Block [{0}]", block.getName()) << '\n';

        for (const auto &i : block) {
            const Instruction *inst = &i;

            // Print instruction
            os << formatv("    Instruction {0}", formatInst(inst)) << '\n';

//            for (unsigned int j = 0; j < inst->getNumOperands(); j++) {
//                Value *op = inst->getOperand(j);
//                SourceVariableMap::const_iterator iterator = mapping.find(op);
//
//                if (iterator != mapping.end()) {
//                    const SourceVariable &var = iterator->getSecond();
//                    os << formatv("      Var {0}", var.first->getName()) << '\n';
//                }
//            }

            // Print dependencies
            printInstDeps(os, inst);
        }
    }
}

void DependenceCollector::printInstDeps(raw_ostream &os, const Instruction *inst) const {
    DependencyMap::const_iterator dependencyIterator = dependencies.find(inst);

    // Quit if there are no dependencies
    if (dependencyIterator == dependencies.end()) {
        return;
    }

    // Get dependencies for instruction from iterator
    const DependencySet &instDependencies = dependencyIterator->second;

    // Loop over the dependencies
    for (const auto &D : instDependencies) {
        const Instruction *dependentInst = D.first.getPointer();
        const BasicBlock *dependentBlock = D.second;
        DependencyType type = D.first.getInt();

        if (dependentInst || dependentBlock) {
            os << "      Depends (" << formatDependencyType(type) << ") on ";
        }
        if (dependentInst) {
            os << formatv("Instruction {0}", formatInst(dependentInst));
        }
        if (dependentInst && dependentBlock) {
            os << " in ";
        }
        if (dependentBlock) {
            os << formatv("Block [{0}]", dependentBlock->getName(), formatDebugLoc(&dependentBlock->front()));
            std::string blockLoc = formatDebugLoc(&dependentBlock->front());
            if (!blockLoc.empty()) {
                os << " ~@ " << blockLoc;
            }
        }
        if (dependentInst || dependentBlock) {
            os << '\n';
        }
    }
}

DependencyMap DependenceCollector::getDependencies() const {
    return dependencies;
}

char DependenceCollector::ID = 0;

static RegisterPass<DependenceCollector> DependenceCollectorPass("checkmerge-memdep", "CheckMerge Memory Dependence", false, true);
