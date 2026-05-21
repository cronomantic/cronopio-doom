# Repro: translator rejects pointer-relocation global initializers (multi-file)

cvm-translate fails on any global whose initializer contains a pointer to
another global (a string literal, a data global, or a function), once that
global is *live* in the llvm-linked module. Single-file builds appear to work
only because the global gets dead-code-eliminated after inlining.

This blocks Crispy Doom's core static tables: S_sfx[], S_music[] (struct arrays
with a `char *name` member), sprnames[], mapnames[]/player_names[] (char*
arrays), etc. — all of which are live in the real engine.

## Reproduce

    cronopio-cc sfa.c sfb.c -o sf.bin

Fails with:

    translator: global 'tbl': unsupported initializer shape
    (or function-pointer initialiser without a definition for the referenced function)

A single-file build of an equivalent program translates fine (the array is
folded/eliminated). The failing IR shape is e.g.:

    @tbl = constant [3 x { ptr, i32 }] [ { ptr @.str, i32 1 }, ... ]
    @.str = private unnamed_addr constant [4 x i8] c"aaa\00"

i.e. an aggregate initializer with a relocation entry (`ptr @.str`).

## Fix needed (translator)

cvm-translate must emit data-relocation entries for global initializers that
reference the address of other globals (strings/data/functions), the same way
it already lays out the globals themselves. Then DOOM's tables serialize and
the port links.
