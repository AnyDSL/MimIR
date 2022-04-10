USAGE:
  thorin [-?|-h|--help] [-v|--version] [-c|--clang <clang>] [-d|--dialect <dialect>] [-D|--dialect-path <path>] [-e|--emit <thorin|md|ll|dot>] [-V|--verbose] [-b|--break <gid>] [-o|--output <prefix>] [<file>]

Display usage information.

OPTIONS, ARGUMENTS:
  -?, -h, --help
  -v, --version           Display version info and exit.
  -c, --clang <clang>     Path to clang executable (default: "which clang").
  -d, --dialect <dialect> Dynamically load dialect [WIP].
  -D, --dialect-path <path>
                          Path to search dialects in.
  -e, --emit <thorin|md|ll|dot>
                          Select emitter. Multiple emitters can be specified simultaneously.
  -V, --verbose           Verbose mode. Multiple -V options increase the verbosity. The maximum is 4.
  -b, --break <gid>       Trigger break-point upon construction of node with global id <gid>. Useful when running in a debugger.
  -o, --output <prefix>   Prefix used for various output files.
