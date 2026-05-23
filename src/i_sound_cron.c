/* Cronopio sound backend.
 *
 * SFX: still a no-op stub (wired in a later step).
 * MUSIC: DOOM MUS / MIDI -> the host's MIDI + SoundFont synth (cron_midi_*).
 * I_RegisterSong converts a MUS lump to MIDI in memory (mus2mid) and parses it
 * (MIDI_LoadMem); a small wall-clock sequencer (I_Cron_UpdateMusic, pumped once
 * per frame from engine_tick) dispatches MIDI events to the host as they fall
 * due. The host synthesises natively, so the VM cost here is just event walking.
 *
 * Timing is integer microseconds (no float on the VM): us_per_tick = tempo /
 * ticks-per-quarter-note; each track tracks the absolute song-us of its pending
 * event; cron_time_ms() drives playback (the headless virtual clock too). */

#include <cronopio.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "doomtype.h"
#include "i_sound.h"
#include "memio.h"
#include "mus2mid.h"
#include "midifile.h"
#include "w_wad.h"
#include "deh_str.h"
#include "m_misc.h"
#include "z_zone.h"

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

/* ---- sfx: DMX DS* lumps -> host PCM voices ----------------------------- */
/* DOOM SFX are 8-bit unsigned mono PCM (DMX format), played straight from the
 * WAD in cart ROM with cron_sample_u8 + cron_pcm — no copy/convert. Each DOOM
 * sound channel maps 1:1 to a host voice (music uses the MIDI synth, so all
 * voices are free for SFX). The host captures the sample descriptor at trigger
 * time, so a single scratch sample slot can be reused for every sound. */

#define CRON_SFX_CHANNELS  16     /* host voice count */
#define CRON_SFX_SLOT      0      /* scratch sample bank, reused per trigger */

int use_sfx_prefix = 1;           /* DOOM prefixes sound lumps with "ds" */

static uint32_t g_sfx_end_ms[CRON_SFX_CHANNELS];  /* when each channel goes idle */

void I_InitSound(GameMission_t mission) { (void)mission; }
void I_ShutdownSound(void) { }

static void sfx_lump_name(sfxinfo_t *sfx, char *buf, size_t n)
{
    if (sfx->link) sfx = sfx->link;
    if (use_sfx_prefix) M_snprintf(buf, n, "ds%s", DEH_String(sfx->name));
    else                M_StringCopy(buf, DEH_String(sfx->name), n);
}

int I_GetSfxLumpNum(sfxinfo_t *sfxinfo)
{
    char buf[9];
    sfx_lump_name(sfxinfo, buf, sizeof(buf));
    return W_CheckNumForName(buf);
}

void I_PrecacheSounds(sfxinfo_t *sounds, int num_sounds)
{
    char buf[9];
    for (int i = 0; i < num_sounds; ++i)
    {
        sfx_lump_name(&sounds[i], buf, sizeof(buf));
        sounds[i].lumpnum = W_CheckNumForName(buf);
    }
}

int I_StartSound(sfxinfo_t *sfxinfo, int channel, int vol, int sep, int pitch)
{
    if (channel < 0 || channel >= CRON_SFX_CHANNELS) return -1;
    int lumpnum = sfxinfo->lumpnum;
    if (lumpnum < 0) return -1;

    byte *data   = (byte *)W_CacheLumpNum(lumpnum, PU_STATIC);  /* ROM, persistent */
    int   lumplen = W_LumpLength(lumpnum);

    /* DMX header: format 0x03, 16-bit rate, 32-bit length; 16 pad bytes each end. */
    if (lumplen < 8 || data[0] != 0x03 || data[1] != 0x00) return -1;
    int rate   = data[2] | (data[3] << 8);
    int length = data[4] | (data[5] << 8) | (data[6] << 16) | (data[7] << 24);
    if (rate <= 0 || length > lumplen - 8 || length <= 48) return -1;

    byte *samples = data + 16;     /* skip the 16-byte lead pad (DMX behaviour) */
    int   slen    = length - 32;   /* and the 16-byte trail pad */
    if (slen <= 0) return -1;

    int pq16 = (pitch <= 0) ? 0x10000 : (pitch * 0x10000 / 128);  /* 128 = native */
    int v    = vol * 255 / 127; if (v < 0) v = 0; if (v > 255) v = 255;
    int pan  = sep - 128;       if (pan < -128) pan = -128; if (pan > 127) pan = 127;

    cron_sample_u8(CRON_SFX_SLOT, (const uint8_t *)samples, slen, rate);
    cron_pcm(channel, CRON_SFX_SLOT, pq16, v, pan, 0);

    /* Track playback end so I_SoundIsPlaying can report channel idle (the host
     * exposes no per-voice query). Duration scales inversely with pitch. */
    int dur_ms = slen * 1000 / rate;
    if (pitch > 0) dur_ms = dur_ms * 128 / pitch;
    g_sfx_end_ms[channel] = cron_time_ms() + (uint32_t)dur_ms;

    return channel;   /* the handle is the channel */
}

void I_StopSound(int channel)
{
    if (channel < 0 || channel >= CRON_SFX_CHANNELS) return;
    cron_snd_stop(channel);
    g_sfx_end_ms[channel] = 0;
}

boolean I_SoundIsPlaying(int channel)
{
    if (channel < 0 || channel >= CRON_SFX_CHANNELS) return false;
    return (int32_t)(g_sfx_end_ms[channel] - cron_time_ms()) > 0;
}

/* The host has no per-voice volume/pan update, so 3D repositioning of an
 * already-playing sound is not applied (sounds keep their trigger-time vol/pan).
 * SFX are short, so this is a minor fidelity loss; a cron_pcm_params syscall
 * could add it later. */
void I_UpdateSoundParams(int channel, int vol, int sep)
{ (void)channel; (void)vol; (void)sep; }

void I_UpdateSound(void) { }   /* host mixes; nothing to pump here */

/* ---- music: MUS/MIDI sequencer feeding the host synth ------------------ */

#define CRON_MUS_MAX_TRACKS 32

typedef struct {
    midi_file_t       *file;
    int                num_tracks;
    midi_track_iter_t *iter[CRON_MUS_MAX_TRACKS];
    midi_event_t      *pending[CRON_MUS_MAX_TRACKS];  /* next event, peeked */
    int            track_us[CRON_MUS_MAX_TRACKS];  /* abs song-us of pending */
    int                done[CRON_MUS_MAX_TRACKS];
    int                ticks_per_qn;
    int            us_per_tick;
    boolean            looping;
} cron_song_t;

static cron_song_t *g_song;
static boolean      g_playing;
static boolean      g_paused;
static uint32_t     g_start_ms;       /* cron_time_ms at playback start */
static uint32_t     g_pause_ms;       /* cron_time_ms when paused */
static int          g_music_vol = 255;

static int us_per_tick_for(int tempo_us, int tpqn)
{
    if (tpqn <= 0) tpqn = 96;
    return (int)tempo_us / tpqn;
}

/* Read track t's next event and schedule its absolute song-us. */
static void track_advance(cron_song_t *s, int t)
{
    if (MIDI_GetNextEvent(s->iter[t], &s->pending[t]))
        s->track_us[t] += (int)s->pending[t]->delta_time * s->us_per_tick;
    else
    {
        s->done[t] = 1;
        s->pending[t] = NULL;
    }
}

static void song_rewind(cron_song_t *s)
{
    s->us_per_tick = us_per_tick_for(500000, s->ticks_per_qn);  /* 120 BPM default */
    for (int t = 0; t < s->num_tracks; ++t)
    {
        MIDI_RestartIterator(s->iter[t]);
        s->track_us[t] = 0;
        s->done[t] = 0;
        track_advance(s, t);
    }
}

static void dispatch_event(cron_song_t *s, midi_event_t *e)
{
    switch (e->event_type)
    {
        case MIDI_EVENT_NOTE_OFF:
        case MIDI_EVENT_NOTE_ON:
        case MIDI_EVENT_AFTERTOUCH:
        case MIDI_EVENT_CONTROLLER:
        case MIDI_EVENT_PITCH_BEND:
            cron_midi_send((int)(e->event_type | e->data.channel.channel),
                           (int)e->data.channel.param1,
                           (int)e->data.channel.param2);
            break;
        case MIDI_EVENT_PROGRAM_CHANGE:
        case MIDI_EVENT_CHAN_AFTERTOUCH:
            cron_midi_send((int)(e->event_type | e->data.channel.channel),
                           (int)e->data.channel.param1, 0);
            break;
        case MIDI_EVENT_META:
            if (e->data.meta.type == MIDI_META_SET_TEMPO && e->data.meta.length == 3)
            {
                int tempo = (e->data.meta.data[0] << 16)
                          | (e->data.meta.data[1] << 8)
                          |  e->data.meta.data[2];
                s->us_per_tick = us_per_tick_for(tempo, s->ticks_per_qn);
            }
            break;
        default:
            break;   /* sysex and other meta ignored */
    }
}

/* Pumped once per frame from engine_tick. Dispatches every event that has
 * fallen due since the last call. */
void I_Cron_UpdateMusic(void)
{
    cron_song_t *s = g_song;
    if (!g_playing || g_paused || !s) return;

    int cur_us = (int)(cron_time_ms() - g_start_ms) * 1000;

    for (;;)
    {
        int best = -1;
        for (int t = 0; t < s->num_tracks; ++t)
        {
            if (s->done[t]) continue;
            if (best < 0 || s->track_us[t] < s->track_us[best]) best = t;
        }
        if (best < 0)   /* every track finished */
        {
            if (s->looping)
            {
                song_rewind(s);
                g_start_ms = cron_time_ms();
            }
            else
            {
                g_playing = false;
                cron_midi_reset();
            }
            return;
        }
        if (s->track_us[best] > cur_us) break;   /* next event not due yet */

        dispatch_event(s, s->pending[best]);
        track_advance(s, best);
    }
}

/* ---- music interface --------------------------------------------------- */

void I_InitMusic(void) { }
void I_ShutdownMusic(void) { I_StopSong(); }

void I_SetMusicVolume(int volume)
{
    /* DOOM passes 0..127 here (snd_MusicVolume scaled); map to the synth's 0..255. */
    if (volume < 0) volume = 0;
    if (volume > 127) volume = 127;
    g_music_vol = volume * 255 / 127;
    cron_midi_volume(g_music_vol);
}

void I_PauseSong(void)
{
    if (g_playing && !g_paused)
    {
        g_paused = true;
        g_pause_ms = cron_time_ms();
        cron_midi_reset();           /* silence held notes while paused */
    }
}

void I_ResumeSong(void)
{
    if (g_playing && g_paused)
    {
        g_start_ms += cron_time_ms() - g_pause_ms;   /* don't count paused time */
        g_paused = false;
    }
}

void *I_RegisterSong(void *data, int len)
{
    byte         *d = (byte *)data;
    midi_file_t  *file = NULL;

    if (IsMus(d, len))
    {
        MEMFILE *in  = mem_fopen_read(d, len);
        MEMFILE *out = mem_fopen_write();
        if (mus2mid(in, out))        /* nonzero == conversion error */
        {
            mem_fclose(in);
            mem_fclose(out);
            return NULL;
        }
        mem_fclose(in);

        void   *midibuf = NULL;
        size_t  midilen = 0;
        mem_get_buf(out, &midibuf, &midilen);
        file = MIDI_LoadMem(midibuf, midilen);   /* copies; out can close after */
        mem_fclose(out);
    }
    else if (IsMid(d, len))
    {
        file = MIDI_LoadMem(d, (size_t)len);
    }

    if (file == NULL) return NULL;

    cron_song_t *s = (cron_song_t *)calloc(1, sizeof(cron_song_t));
    if (s == NULL) { MIDI_FreeFile(file); return NULL; }

    s->file         = file;
    s->num_tracks   = (int)MIDI_NumTracks(file);
    if (s->num_tracks > CRON_MUS_MAX_TRACKS) s->num_tracks = CRON_MUS_MAX_TRACKS;
    s->ticks_per_qn = (int)MIDI_GetFileTimeDivision(file);
    for (int t = 0; t < s->num_tracks; ++t)
        s->iter[t] = MIDI_IterateTrack(file, (unsigned int)t);

    return s;
}

void I_UnRegisterSong(void *handle)
{
    cron_song_t *s = (cron_song_t *)handle;
    if (s == NULL) return;
    if (g_song == s) { g_playing = false; g_song = NULL; }
    for (int t = 0; t < s->num_tracks; ++t)
        if (s->iter[t]) MIDI_FreeIterator(s->iter[t]);
    if (s->file) MIDI_FreeFile(s->file);
    free(s);
}

void I_PlaySong(void *handle, boolean looping)
{
    g_song = (cron_song_t *)handle;
    if (g_song == NULL) { g_playing = false; return; }

    g_song->looping = looping;
    cron_midi_reset();
    song_rewind(g_song);
    g_start_ms = cron_time_ms();
    g_paused   = false;
    g_playing  = true;
}

void I_StopSong(void)
{
    g_playing = false;
    cron_midi_reset();
}

boolean I_MusicIsPlaying(void) { return g_playing; }

boolean IsMid(byte *mem, int len) { return len > 4 && memcmp(mem, "MThd", 4) == 0; }
boolean IsMus(byte *mem, int len) { return len > 4 && memcmp(mem, "MUS\x1a", 4) == 0; }

void I_BindSoundVariables(void) { }

void I_SetOPLDriverVer(opl_driver_ver_t ver) { (void)ver; }
void I_OPL_DevMessages(char *msg, size_t len) { if (msg && len) msg[0] = '\0'; }
void I_InitTimidityConfig(void) { }
