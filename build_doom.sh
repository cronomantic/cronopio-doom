#!/usr/bin/env bash
# Build the Cronopio DOOM cartridge (Crispy Doom 7.1 -> CronoVM .bin).
#
# Multi-file build: cvm-cc compiles every .c to bitcode, llvm-links them, then
# cvm-translate lowers to doom.bin. The IWAD is baked into ROM via --rom.
set -u

ROOT="/e/proyectos/cronopio-doom"
CR="$ROOT/third_party/crispy-doom/src"
SDK="/e/proyectos/Cronopio/sdk"
RT="/e/proyectos/Cronopio/external/CronoVM/runtime/lib"
CC="/e/proyectos/Cronopio/build/tools/cronopio-cc/cronopio-cc.exe"

IWAD="${1:-$ROOT/wads/freedoom1.wad}"
OUT="${2:-$ROOT/doom.bin}"

# --- include dirs (cronopio-cc takes -I and its arg as separate tokens) ----
INCS=(
  -I "$ROOT/compat"
  -I "$SDK/include"
  -I "$RT"
  -I "$CR"
  -I "$CR/doom"
)

# --- DOOM sources we keep (doom/*) -----------------------------------------
DOOM_KEEP=(
  d_items d_main d_net d_pwad
  deh_ammo deh_bexincl deh_bexpars deh_bexptr deh_bexstr deh_cheat deh_doom
  deh_frame deh_misc deh_ptr deh_sound deh_thing deh_weapon
  doom_icon doomdef doomstat dstrings
  f_finale f_wipe g_game hu_lib hu_stuff info
  m_crispy m_menu m_random
  p_bexptr p_blockmap p_ceilng p_doors p_enemy p_extnodes p_extsaveg p_floor
  p_inter p_lights p_map p_maputl p_mobj p_plats p_pspr p_saveg p_setup
  p_sight p_spec p_switch p_telept p_tick p_user
  r_bmaps r_bsp r_data r_draw r_main r_plane r_segs r_sky r_swirl r_things
  s_musinfo s_sound sounds st_lib st_stuff v_snow wi_stuff
)

# --- common sources we keep (src/*) ----------------------------------------
SRC_KEEP=(
  a11y aes_prng crispy d_event d_iwad d_mode
  deh_io deh_main deh_mapping deh_str deh_text
  m_argv m_bbox m_cheat m_config m_controls m_misc memio
  p_rejectpad sha1 tables v_diskicon v_trans v_video
  w_checksum w_file w_main w_merge w_wad z_zone
  i_truecolor
)

SOURCES=()
for f in "${DOOM_KEEP[@]}"; do SOURCES+=("$CR/doom/$f.c"); done
for f in "${SRC_KEEP[@]}";  do SOURCES+=("$CR/$f.c"); done

# --- our port-local files ---------------------------------------------------
PORT=(
  "$ROOT/src/doom_cart.c"
  "$ROOT/src/platform.c"
  "$ROOT/src/engine_cron.c"
  "$ROOT/src/i_video_cron.c"
  "$ROOT/src/i_system_cron.c"
  "$ROOT/src/i_timer_cron.c"
  "$ROOT/src/i_sound_cron.c"
  "$ROOT/src/i_joystick_cron.c"
  "$ROOT/src/i_input_cron.c"
  "$ROOT/src/net_stub_cron.c"
  "$ROOT/src/statdump_cron.c"
  "$ROOT/src/i_glob_cron.c"
  "$ROOT/src/w_file_rom.c"
  "$ROOT/src/am_map_stub.c"
  "$ROOT/src/d_loop_cron.c"
  "$ROOT/src/m_fixed_cvm.c"
  "$SDK/lib/cvm_libc.c"
)

echo "[build] $(( ${#SOURCES[@]} + ${#PORT[@]} )) translation units -> $OUT"
echo "[build] IWAD: $IWAD"

"$CC" \
  "${INCS[@]}" \
  "${SOURCES[@]}" \
  "${PORT[@]}" \
  --rom="$IWAD" \
  --heap-reserve=96M \
  --stack-reserve=4M \
  -o "$OUT"
