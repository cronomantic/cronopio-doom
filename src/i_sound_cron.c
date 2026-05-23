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

/* ---- sfx (still stubbed) ----------------------------------------------- */
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
