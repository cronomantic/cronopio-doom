/* Cronopio sound backend — full no-op stub of the i_sound.h interface.
 *
 * Replaces i_sound.c plus all the SDL/OPL/FluidSynth music backends. Audio is
 * silent for now; s_sound.c drives these and tolerates a -1/false device.
 */
#include "doomtype.h"
#include "i_sound.h"

/* config vars normally bound by I_BindSoundVariables */
int   snd_sfxdevice = 0;       /* SNDDEVICE_NONE */
int   snd_musicdevice = 0;
int   snd_samplerate = 44100;
int   snd_cachesize = 64 * 1024 * 1024;
int   snd_maxslicetime_ms = 28;
char *snd_musiccmd = "";
int   snd_pitchshift = 0;
char *snd_dmxoption = "";
int   use_libsamplerate = 0;
float libsamplerate_scale = 0.65f;

/* ---- sfx --------------------------------------------------------------- */
void I_InitSound(GameMission_t mission) { (void)mission; }
void I_ShutdownSound(void) { }
int  I_GetSfxLumpNum(sfxinfo_t *sfxinfo) { (void)sfxinfo; return 0; }
void I_UpdateSound(void) { }
void I_UpdateSoundParams(int channel, int vol, int sep)
{ (void)channel; (void)vol; (void)sep; }
int  I_StartSound(sfxinfo_t *sfxinfo, int channel, int vol, int sep, int pitch)
{ (void)sfxinfo; (void)channel; (void)vol; (void)sep; (void)pitch; return -1; }
void I_StopSound(int channel) { (void)channel; }
boolean I_SoundIsPlaying(int channel) { (void)channel; return false; }
void I_PrecacheSounds(sfxinfo_t *sounds, int num_sounds)
{ (void)sounds; (void)num_sounds; }

/* ---- music ------------------------------------------------------------- */
void I_InitMusic(void) { }
void I_ShutdownMusic(void) { }
void I_SetMusicVolume(int volume) { (void)volume; }
void I_PauseSong(void) { }
void I_ResumeSong(void) { }
void *I_RegisterSong(void *data, int len) { (void)data; (void)len; return (void *)0; }
void I_UnRegisterSong(void *handle) { (void)handle; }
void I_PlaySong(void *handle, boolean looping) { (void)handle; (void)looping; }
void I_StopSong(void) { }
boolean I_MusicIsPlaying(void) { return false; }

boolean IsMid(byte *mem, int len) { (void)mem; (void)len; return false; }
boolean IsMus(byte *mem, int len) { (void)mem; (void)len; return false; }

void I_BindSoundVariables(void) { }

void I_SetOPLDriverVer(opl_driver_ver_t ver) { (void)ver; }
void I_OPL_DevMessages(char *msg, size_t len) { if (msg && len) msg[0] = '\0'; }
void I_InitTimidityConfig(void) { }
