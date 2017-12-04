/**
 * \file DependenceCollector.cpp
 * \author Jan-Jelle Kester
 *
 * LLVM analysis pass that collects the results of the built-in memory dependence analysis and exposes this to a simple
 * interface.
 *
 * \note Code heavily inspired by and partially copied from the native llvm/lib/Analysis/MemDepPrinter.cpp file.
 */
#include <llvm/Pass.h>
#include <llvm/ADT/SetVector.h>
#include <llvm/Analysis/MemoryDependenceAnalysis.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/Support/FormatVariadic.h>

using namespace llvm;

namespace {

    /**
     * Analysis pass which, for every function, accumulates the memory dependencies of each instruction.
     */
    struct DependenceCollector : public FunctionPass {

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
        // The (optional) dependency (nullptr if  with an optional block (nullptr if the dependency is local to the block of the
        // instruction).
        typedef std::pair<Dependency, const BasicBlock *> DependencyPair;
        // A set of dependencies
        typedef SmallSetVector<DependencyPair, 4> DependencySet;
        // The dependencies per instruction
        typedef DenseMap<const Instruction *, DependencySet> DependencyMap;

        // Metadata
        typedef SmallVector<std::pair<unsigned, MDNode *>, 4> MDVector;

        DependencyMap dependencies;
        Function *function;

        static char ID;

        DependenceCollector() : FunctionPass(ID) {
            this->function = nullptr;
        };

        bool runOnFunction(Function &function) override;
        void print(raw_ostream &os, const Module * = nullptr) const override;

        /**
         * Prints the dependencies of the given instruction to the given output stream.
         *
         * @param os The output stream to print to.
         * @param inst The instruction to print the dependencies of.
         */
        void printInstDeps(raw_ostream &os, const Instruction *inst) const;

        // Clean up memory
        void releaseMemory() override {
            this->dependencies.clear();
            this->function = nullptr;
        }

        // Dependencies and behavior of this analysis
        void getAnalysisUsage(AnalysisUsage &usage) const override {
            usage.setPreservesAll();
            usage.addRequired<MemoryDependenceWrapperPass>();
        }

    private:
        /**
         * Builds a combination of the instruction and its dependency type from a dependency result.
         *
         * @param result The query result to define the dependency for.
         */
        static Dependency buildDependency(MemDepResult result) {
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

        /**
         * Builds a pair of a dependency and the block it appears in.
         *
         * @param dependency The dependency to optionally pair with a block.
         * @param block The block to optionally pair with a dependency.
         */
        static DependencyPair buildDependencyPair(Dependency dependency, const BasicBlock * block) {
            return static_cast<DependencyPair>(std::make_pair(dependency, block));
        }

        /**
         * String formats the given instruction with some debug information.
         *
         * @param inst The instruction to format.
         * @return A string representation of the instruction.
         */
        static std::string formatInst(const Instruction *inst) {
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

        /**
         * String formats the debug location of the given instruction. May return the empty string if no location is
         * available.
         *
         * @param inst The instruction to format the location of.
         * @return A string representation of the original location of the instruction.
         */
        static std::string formatDebugLoc(const Instruction *inst) {
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

        static std::string formatDependencyType(const DependencyType type) {
            switch (type) {
                case DependencyType::NonFuncLocal: return "non-local";
                case DependencyType::Clobber: return "clobber";
                case DependencyType::Def: return "def";
                default: return "unknown";
            }
        }
    };
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

    // Print function name
    os << formatv("Function [{0}]", function->getName()) << '\n';

    for (const auto &block : *function) {
        // Print basic block
        os << formatv("  Block [{0}]", block.getName()) << '\n';

        for (const auto &i : block) {
            const Instruction *inst = &i;

            // Print instruction
            os << formatv("    Instruction {0}", formatInst(inst)) << '\n';

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

char DependenceCollector::ID = 0;

static RegisterPass<DependenceCollector> DependenceCollectorPass("checkmerge-memdep", "CheckMerge memory dependence", false, false);
