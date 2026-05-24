#!/usr/bin/env bash
# Build one Cronopio DOOM cartridge per game you have a WAD for.
#
# Scans wads/ and builds a nicely-named .crom per IWAD into dist/. Supplementary
# PWADs are supported via a manifest (wads/carts.txt) or on the command line;
# they're merged into the IWAD (see tools/wadtool.py) and baked as one ROM.
#
# Usage:
#   bash make_carts.sh                       # auto: one cart per IWAD in wads/
#   bash make_carts.sh NAME IWAD [PWAD...]   # one custom cart (paths rel. to wads/)
#
# With no args, if wads/carts.txt exists it drives the build instead of the
# auto scan. Manifest lines: "<name> <iwad> [pwad...]" (paths relative to wads/;
# blank lines and #-comments ignored).
set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WADS="$ROOT/wads"
DIST="$ROOT/dist"

# Shared controls help baked into every DOOM cart's metadata (12-button pad).
DOOM_CONTROLS="D-pad: mover/girar | L/R: strafe | A: disparar | B: usar | X/Y: arma | Start: menu | Select: mapa"
WADTOOL_SRC="$ROOT/tools/wadtool.c"
WADTOOL="$ROOT/tools/wadtool.exe"
BUILD="$ROOT/build_doom.sh"

mkdir -p "$DIST"

# wadtool is a tiny C program (no scripting-language dependency — building a
# cart already needs a C compiler). Compile it on first use / when out of date.
ensure_wadtool() {
  if [[ -x "$WADTOOL" && "$WADTOOL" -nt "$WADTOOL_SRC" ]]; then return 0; fi
  local cc
  for cc in "${CC:-}" cc clang gcc; do
    [[ -n "$cc" ]] || continue
    command -v "$cc" >/dev/null 2>&1 || continue
    echo "[carts] compiling wadtool ($cc)"
    if "$cc" -O2 -o "$WADTOOL" "$WADTOOL_SRC"; then return 0; fi
  done
  echo "[carts] ERROR: no C compiler (set \$CC) to build tools/wadtool.c" >&2
  return 1
}
ensure_wadtool || exit 1

# Resolve a WAD path: absolute/relative as given, else under wads/.
resolve_wad() {
  local w="$1"
  if [[ -f "$w" ]]; then echo "$w"; return 0; fi
  if [[ -f "$WADS/$w" ]]; then echo "$WADS/$w"; return 0; fi
  return 1
}

# build_one NAME IWAD [PWAD...] — merge if PWADs, then build dist/NAME.bin.
build_one() {
  local name="$1"; shift
  local iwad; iwad="$(resolve_wad "$1")" || { echo "  ! IWAD not found: $1" >&2; return 1; }
  shift
  local out="$DIST/$name.crom"
  local rom="$iwad"

  if (( $# > 0 )); then
    local pwads=()
    local p
    for p in "$@"; do
      local rp; rp="$(resolve_wad "$p")" || { echo "  ! PWAD not found: $p" >&2; return 1; }
      pwads+=("$rp")
    done
    rom="$DIST/$name.merged.wad"
    echo "  merging IWAD + ${#pwads[@]} PWAD(s) -> $(basename "$rom")"
    "$WADTOOL" merge "$rom" "$iwad" "${pwads[@]}" || return 1
  fi

  echo "  building $name.crom (rom: $(basename "$rom"))"
  CART_TITLE="$name" CART_CONTROLS="$DOOM_CONTROLS" \
    bash "$BUILD" "$rom" "$out" >/dev/null 2>"$DIST/$name.build.log" || {
    echo "  ! build failed — see $DIST/$name.build.log" >&2; return 1; }
  echo "  -> $out"
  return 0
}

# ---- command-line one-off -------------------------------------------------
if (( $# >= 2 )); then
  echo "[carts] custom cart: $1"
  build_one "$@"
  exit $?
fi
if (( $# == 1 )); then
  echo "usage: bash make_carts.sh NAME IWAD [PWAD...]" >&2
  exit 2
fi

# ---- manifest-driven ------------------------------------------------------
ok=0; fail=0
if [[ -f "$WADS/carts.txt" ]]; then
  echo "[carts] driving from wads/carts.txt"
  while read -r name iwad rest || [[ -n "$name" ]]; do
    [[ -z "$name" || "$name" == \#* ]] && continue
    echo "[carts] $name"
    # shellcheck disable=SC2086
    if build_one "$name" "$iwad" $rest; then ok=$((ok+1)); else fail=$((fail+1)); fi
  done < "$WADS/carts.txt"
  echo "[carts] done: $ok built, $fail failed"
  exit $(( fail > 0 ))
fi

# ---- auto: one cart per runnable IWAD in wads/ ----------------------------
echo "[carts] auto-scanning $WADS"
shopt -s nullglob
declare -A used
for wad in "$WADS"/*.wad "$WADS"/*.WAD; do
  [[ -f "$wad" ]] || continue
  info="$("$WADTOOL" identify "$wad" 2>/dev/null)" || { echo "  ? skipping (bad WAD): $(basename "$wad")"; continue; }
  type="$(cut -f1 <<<"$info")"; game="$(cut -f2 <<<"$info")"; runnable="$(cut -f3 <<<"$info")"
  base="$(basename "$wad")"
  if [[ "$type" != "IWAD" ]]; then
    echo "  - skipping PWAD (needs an IWAD via manifest/CLI): $base"
    continue
  fi
  if [[ "$runnable" != "1" ]]; then
    echo "  - skipping $base: '$game' is not runnable on the crispy-doom engine"
    continue
  fi
  name="$game"
  [[ "$name" == "unknown" ]] && name="$(basename "${wad%.*}")"
  # Disambiguate if two WADs map to the same name.
  if [[ -n "${used[$name]:-}" ]]; then
    name="${name}-$(basename "${wad%.*}")"
  fi
  used[$name]=1
  echo "[carts] $base -> $name ($game)"
  if build_one "$name" "$wad"; then ok=$((ok+1)); else fail=$((fail+1)); fi
done
echo "[carts] done: $ok built, $fail failed"
exit $(( fail > 0 ))
