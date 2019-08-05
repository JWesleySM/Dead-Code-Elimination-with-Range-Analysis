# Dead Code Elimination with Range Analysis

This project is a LLVM pass that performs dead code elimination based on the results of a range analysis implemented by [Victor Campos](https://github.com/vhscampos) (https://github.com/vhscampos/range-analysis). It was made in LLVM 6.0.0

Before use it, [download and install](http://releases.llvm.org/) LLVM.

To compile the pass:

1. unpack the directory in /llvm/lib/Transforms
2. edit the file /llvm/lib/Transforms/CMakeLists.txt: add the line add_subdirectory(Dead-Code-Elimination-with-Range-Analysis)
3. goto /llvm/build directory
4. type ```make RangeAnalysis``` (or just type ```make``` to compile the entire LLVM)

To execute the pass:

   - execute the script deadcode_elimination_with_RA.sh
   - you must change the command line flags, CLANG, OPT and RANGE_LIB according with your path llvm configuration 
   
*if you are running on Mac OS, the extension of the shared library will be .dylib instead .so
