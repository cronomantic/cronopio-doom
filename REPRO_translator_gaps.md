# Translator gaps blocking the DOOM port (for the parent agent)

Two CronoVM **translator** limitations now block the build. Both are
beyond what the port can paper over in source (they affect hundreds of
optimizer-generated sites / dozens of large functions). Toolchain paths:
clang 22.1.6, cvm-cc / cvm-translate at the usual build/_cvm paths.

## Gap 1 — standard min/max/abs intrinsics are not lowered

clang at `-O1` (the level cvm-cc uses; `-O0` reintroduces i64, `-O2` adds
SIMD) folds ordinary `a<b?a:b`, `a>b?a:b`, and `abs()` idioms into the
LLVM intrinsics `llvm.smax/smin/umax/umin.iN` and `llvm.abs.iN`. The
translator:
  - **i16 variant**: errors `intrinsic 'llvm.smax.i16' not yet lowered`.
  - **i32 variant**: silently emits it as an *undefined external function*
    and still writes the .bin — which would TRAP at run time.

Census across the kept DOOM sources (count of occurrences):
    10 llvm.abs.i32   3 llvm.smax.i16   116 llvm.smax.i32
   124 llvm.smin.i32   9 llvm.umax.i32   19 llvm.umin.i32
They appear in ~38 files (every gameplay/render TU). Lowering is trivial:
  smax(a,b)=a>b?a:b  smin=a<b?a:b  umax/umin = unsigned compare  abs=x<0?-x:x

Minimal repro:
    cat > t.c <<'EOF'
    #include <stdlib.h>
    int  mx(int a,int b){return a>b?a:b;}      /* llvm.smax.i32 */
    int  mn(int a,int b){return a<b?a:b;}      /* llvm.smin.i32 */
    short ms(short a,short b){return a>b?a:b;} /* llvm.smax.i16 -> ERRORS */
    int  ab(int a){return abs(a);}             /* llvm.abs.i32  */
    EOF
    clang --target=i386-elf -ffreestanding -emit-llvm -O1 -c t.c -o t.bc
    cvm-translate t.bc -o t.bin    # errors on smax.i16; i32 ones become extern stubs

## Gap 2 — register allocator has no spill path (~254 registers)

`cvm-translate` allocates one register per live SSA value with a hard
~254-register file and **no spill**. Large DOOM functions exceed it:
`ran out of registers (R254/R255 are reserved...)`. Hit so far (each fixed
in source by extracting noinline helpers, but it keeps recurring):
D_DoomMain, G_BuildTiccmd, M_Responder, and now **P_TouchSpecialThing**;
many render functions (R_RenderPlayerView, R_DrawColumn paths,
P_SetupLevel, etc.) will follow. Note the budget is *smaller in the linked
module* than when a TU is translated standalone — a function that passes
alone (e.g. 828 instrs) can still fail after llvm-link, so globals appear
to consume the same register space. A real fix needs spill-to-stack (or a
much larger register file); splitting every large function by hand is not
tractable.

Minimal repro: any function with >~250 simultaneously-live values; e.g.
translate p_inter.c (P_TouchSpecialThing) from the DOOM tree, or
g_game.c's G_BuildTiccmd before the [cronopio] splits.
