USAGE:
  thorin [-?|-h|--help] [-c|--clang <clang>] [-l|--emit-llvm] [-v|--version] [-d|--dialect <dialect>] [-D|--dialect-search-path <path>] [<input file>]

Display usage information.

OPTIONS, ARGUMENTS:
  -?, -h, --help          
  -c, --clang <clang>     path to clang executable (default: {})
  -l, --emit-llvm         emit LLVM
  -v, --version           display version info and exit
  -d, --dialect <dialect> dynamically load dialect [WIP]
  -D, --dialect-search-path <path>
                          path to search dialects in
  <input file>            The input file. Use '-' to read from stdin.

