# CheckMerge LLVM Analysis Pass

The CheckMerge LLVM Analysis Pass is a dynamically loadable module for the LLVM compiler.
It provides an analysis pass than can be executed using the LLVM optimizer which provides analysis data for
CheckMerge parsers with LLVM support.

See also the [CheckMerge repository](https://github.com/jjkester/checkmerge).

At the time of writing this pass is only used by the Clang plugin for CheckMerge that provides C support.

## Building the pass

For building the pass LLVM source code must be present on the build system.
The CMake build script uses the `find_package` command to import the requirements from LLVM.
(Therefore, the LLVM project should be discoverable by CMake.)

To build the shared object containing the analysis pass, run the following commands from the root directory of this
repository. Replace `${BUILD_DIR}` with the directory you want to write the build to or set the `BUILD_DIR` 
environment variable.

```bash
mkdir "$BUILD_DIR"
cmake --build "${BUILD_DIR}"
```

Of course Cmake compatible user interfaces like [CLion](https://www.jetbrains.com/clion/) should be able to build the
library as well.

## Running the pass

The pass can be executed using the LLVM optimizer. **Debug information on the source is required.**
An example command for this is given below.

Given a program compiled to LLVM IR in the file `program.ll`, the analysis pass can be executed with the following
command. The `${BUILD_DIR}` variable should reference the same directory as before.
Be sure to replace `program.ll` with your own program.

```bash
opt -analyze -load="${BUILD_DIR}/checkmerge/LLVMCheckMerge.so" -checkmerge program.ll
```

A summary of the executed tasks will be printed to the standard output. The analysis result will be saved in the same
directory as the source program file. The name will be that of the program input file with `.cm` appended.
For example, the analysis for `program.ll` will be saved in `program.ll.cm`. The file format is based on YAML, so feel
free to open and read it.

CheckMerge will expect the `program.ll` (which is does not need) and the `program.ll.cm` files in the same directory as
the original source file.

### Compiling C to LLVM IR with debug information

To compile C source code to LLVM IR with debug information, run the following command.
Be sure to replace `program.c` with the name of your actual program.

CheckMerge works best without any code optimizations.

```bash
clang -S -O0 -g -emit-llvm program.c
```

The resulting program is saved in the `program.ll` file, or a name similar to the name of your program.
