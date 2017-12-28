#include <llvm/Pass.h>
#include <sstream>
#include <llvm/Support/FormatVariadic.h>
#include <fstream>
#include "DependenceCollector.h"
#include "SourceVariableMapper.h"

using namespace llvm;

namespace {

    class line {
        std::string data;
    public:
        friend std::istream &operator>>(std::istream &is, line &l) {
            std::getline(is, l.data);
            return is;
        }

        operator std::string() const { return data; }
    };

    struct CheckMergePrinter : public FunctionPass {

        static char ID;

        Function *function;
        DependencyMap dependencies;
        SourceVariableMap variables;

        CheckMergePrinter() : FunctionPass(ID) {
            this->function = nullptr;
        }

        // Pass implementation
        bool runOnFunction(Function &F) override;

        // Printer
        void print(raw_ostream &os, const Module *module) const override;

        // Define requirements and behavior
        void getAnalysisUsage(AnalysisUsage &usage) const override;

        // Initialize pass
        bool doInitialization(Module &module) override;

        bool doFinalization(Module &module) override;

    private:

        std::string filename;
        std::ofstream fileStream;

        std::vector<const Instruction *> instructions;

        /**
         * Prepends indentation of the given size to the given string.
         *
         * @param str The string to format.
         * @return The indented string.
         */
        static std::string indentLine(const std::string &str) {
            return std::string(2, ' ').append(str);
        }

        /**
         * Prepends indentation of the given size to all lines of the given string.
         *
         * @param str The string to format.
         * @return The indented string.
         */
        static std::string withIndent(const std::string &str) {
            std::istringstream input(str);
            std::ostringstream output;

            // Indent all lines
            std::transform(
                    std::istream_iterator<line>(input),
                    std::istream_iterator<line>(),
                    std::ostream_iterator<std::string>(output, "\n"),
                    CheckMergePrinter::indentLine
            );

            return output.str();
        }

        std::string formatFunction(Function &function) const {
            std::stringstream out;

            DISubprogram *subprogram = function.getSubprogram();

            out << formatIdentifier(function) << ':' << '\n';
            out << withIndent(
                    formatv(
                            "name: \"{0}\"\nmodule: \"{1}\"\nlocation: \"{2}\"",
                            subprogram != nullptr ? subprogram->getName() : function.getName(),
                            function.getParent()->getName(),
                            subprogram != nullptr ? formatLocation(subprogram->getFilename(), subprogram->getLine()) : "~"
                    )) << '\n';

            for (BasicBlock &block : function) {
                out << withIndent(formatBasicBlock(block)) << '\n';
            }

            return out.str();
        }

        std::string formatBasicBlock(BasicBlock &block) const {
            std::stringstream out;

            out << formatIdentifier(block) << ':' << '\n';

            for (Instruction &instruction : block) {
                out << withIndent(formatInstruction(instruction));
            }

            return out.str();
        }

        std::string formatInstruction(Instruction &instruction) const {
            std::stringstream out;
            std::stringstream nout;

            const DebugLoc &loc = instruction.getDebugLoc();

            out << formatv("- {0}:", formatIdentifier(instruction)).str() << '\n';
            nout << withIndent(formatv(
                    "opcode: {0}\nlocation: \"{1}\"",
                    instruction.getOpcodeName(),
                    bool(loc) ? formatLocation(loc.getLine(), loc.getCol()) : ""
            ));

            // Source variable
            auto variableIter = this->variables.find(&instruction);

            if (variableIter.operator!=(this->variables.end())) {
                SourceVariable variable = variableIter->second;

                nout << withIndent("variable:");
                nout << withIndent(withIndent(formatv(
                        "name: \"{0}\"\nlocation: \"{1}\"",
                        variable.first->getName(),
                        bool(variable.second) ? formatLocation(variable.second->getLine(), variable.second->getCol()) : ""
                )));
            }

            // Dependencies
            auto dependencyIter = this->dependencies.find(&instruction);

            if (dependencyIter.operator!=(this->dependencies.end())) {
                DependencySet dependencies = dependencyIter->second;

                nout << withIndent("dependencies:");

                for (DependencyPair dependencyPair : dependencies) {
                    std::string dependencyRef;
                    std::string dependencyType;

                    if (dependencyPair.first.getPointer() != nullptr) {
                        dependencyRef = formatIdentifier((Instruction &) *dependencyPair.first.getPointer());
                        dependencyType = formatDepType(&instruction, dependencyPair.first);
                    } else if (dependencyPair.second != nullptr) {
                        dependencyRef = formatIdentifier((BasicBlock &) *dependencyPair.second);
                        dependencyType = "Unknown";
                    }

                    if (!dependencyRef.empty()) {
                        nout << indentLine(indentLine(formatv(R"("*{0}": "{1}")", dependencyRef, dependencyType))) << '\n';
                    }
                }
            }

            out << withIndent(nout.str());

            return out.str();
        }

        /**
         * Formats a location string for a source code location.
         *
         * @param filename The name of the file.
         * @param line The line number.
         * @param col The column number.
         * @return The location string with the given properties.
         */
        static std::string formatLocation(const std::string &filename, const int &line = 0, const int &col = 0) {
            return formatv("{0}:{1}:{2}", filename, line, col);
        }

        /**
         * Formats a location string for a source code location.
         *
         * @param line The line number.
         * @param col The column number.
         * @return The location string with the given properties.
         */
        static std::string formatLocation(const int &line = 0, const int &col = 0) {
            return formatv(":{0}:{1}", line, col);
        }

        /**
         * Formats a generic output file identifier.
         *
         * @param prefix The prefix of the identifier.
         * @param descriptor The descriptor of the object to identify.
         * @return The identifier with the given properties.
         */
        static std::string formatIdentifier(const std::string &prefix, const std::string &descriptor) {
            return prefix + "." + descriptor;
        }

        /**
         * Formats the output file identifier for the given function.
         *
         * @param function The function to get the identifier for.
         * @return The identifier of the given function.
         */
        static std::string formatIdentifier(Function &function) {
            return formatIdentifier("function", function.getName());
        }

        /**
         * Formats the output file identifier for the given basic block.
         *
         * @param block The basic block to get the identifier for.
         * @return The identifier of the given basic block.
         */
        static std::string formatIdentifier(BasicBlock &block) {
            return formatIdentifier("block", block.getName());
        }

        /**
         * Formats the output file identifier for the given instruction.
         *
         * @param instruction The instruction to get the identifier for.
         * @return The identifier of the given instruction.
         */
        std::string formatIdentifier(Instruction &instruction) const {
            auto iterator = std::find(instructions.begin(), instructions.end(), &instruction);
            assert(iterator != instructions.end());

            return formatIdentifier("instruction", formatv("{0}", std::distance(instructions.begin(), iterator)));
        }

        std::string formatDepType(Instruction *inst, Dependency dependency) const {
            const Instruction *depInst = dependency.getPointer();

            std::string before = "U", after = "U";

            if (inst->mayReadFromMemory()) {
                after = "R";
            } else if (inst->mayWriteToMemory()) {
                after = "W";
            }

            if (depInst->mayWriteToMemory()) {
                before = "W";
            } else if (depInst->mayReadFromMemory()) {
                before = "R";
            }

            return formatv("{0}A{1}", after, before).str();
        }
    };

}

void CheckMergePrinter::getAnalysisUsage(AnalysisUsage &usage) const {
    usage.setPreservesAll();
    usage.addRequired<DependenceCollector>();
    usage.addRequired<SourceVariableMapper>();
}

bool CheckMergePrinter::runOnFunction(Function &F) {
    this->function = &F;
    this->instructions.clear();

    for (BasicBlock &B : F) {
        for (Instruction &I : B) {
            this->instructions.push_back(&I);
        }
    }

    // Define analysis results
    dependencies = getAnalysis<DependenceCollector>().getDependencies();
    variables = getAnalysis<SourceVariableMapper>().getMapping();

    // Write to file
    if (this->fileStream.is_open()) {
        fileStream << formatFunction(F);
    }

    // No modifications so return false
    return false;
}

void CheckMergePrinter::print(raw_ostream &os, const Module *module) const {
    std::ostringstream out;

    unsigned long dependencyCount = 0;

    for (auto entry : this->dependencies) {
        dependencyCount += entry.second.size();
    }

    out << formatv("Instructions:    {0}", this->instructions.size()).str() << '\n';
    out << formatv("Variables:       {0}", this->variables.size()).str() << '\n';
    out << "Dependencies:" << '\n';
    out << withIndent(formatv("Instructions:  {0}", this->dependencies.size()));
    out << withIndent(formatv("Total:         {0}", dependencyCount));
    out << '\n';
    out << formatv("Written CheckMerge analysis data to file {0}", this->filename).str();
    os << withIndent(out.str());
}

bool CheckMergePrinter::doInitialization(Module &module) {
    const std::string &basename = module.getSourceFileName();
    this->filename = basename.substr(0, basename.find_last_of('.')) + ".ll.cm";
    this->fileStream.open(filename);
}

bool CheckMergePrinter::doFinalization(Module &module) {
    if (this->fileStream.is_open()) {
        this->fileStream.close();
    }
}

char CheckMergePrinter::ID = 0;

static RegisterPass<CheckMergePrinter> CheckMergePrinterPass("checkmerge", "CheckMerge Processing", false, true);
