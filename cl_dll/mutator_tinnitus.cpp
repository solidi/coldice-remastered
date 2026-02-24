//
// mutator_tinnitus.cpp
//
// Tinnitus mutator - client side
//
// Hooks EV_PlaySound so that all game sounds are reduced to 5% volume while
// the mutator is active, and plays a looping buzz on CHAN_STATIC at full volume.
//
// The server activates / deactivates the effect via CLIENT_COMMAND:
//   "tinnitus_start"   -- enable muffling and begin the buzz loop
//   "tinnitus_stop"    -- silence the buzz and restore normal volume
//
// Sound requirement:
//   sound/ambience/tinnitus_buzz.wav  must be present on the client and server.
//   The WAV must have SMPL loop-point markers so the engine loops it on
//   CHAN_STATIC without extra re-trigger logic.
//

#include "hud.h"
#include "cl_util.h"
#include "event_api.h"
#include <string.h>

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

#define TINNITUS_SOUND      "tinnitus_buzz.wav"
#define TINNITUS_VOLUME     0.85f   // buzz volume (world sounds are crushed, this is "loud")
#define TINNITUS_DUCK       0.05f   // all other sounds are multiplied by this
#define TINNITUS_DURATION   41.0    // length of tinnitus_buzz.wav in seconds

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

bool   g_bTinnitusActive = false;
static double s_flNextPlay = 0.0;  // engine time at which to replay the sound

// Saved original function pointer - nullptr until Tinnitus_Hook() is called
static void (*s_EV_PlaySound_Original)(int ent, float *origin, int channel, const char *sample, float volume, float attenuation, int fFlags, int pitch) = nullptr;

// ---------------------------------------------------------------------------
// Hook: replacement for gEngfuncs.pEventAPI->EV_PlaySound
// ---------------------------------------------------------------------------

static void EV_PlaySound_Tinnitus(int ent, float *origin, int channel, const char *sample, float volume, float attenuation, int fFlags, int pitch)
{
    if (g_bTinnitusActive)
    {
        // Let the buzz loop through unmodified so it stays at full volume
        if (strcmp(sample, TINNITUS_SOUND) == 0)
        {
            s_EV_PlaySound_Original(ent, origin, channel, sample, volume, attenuation, fFlags, pitch);
            return;
        }

        // Muffle everything else
        volume *= TINNITUS_DUCK;
    }

    s_EV_PlaySound_Original(ent, origin, channel, sample, volume, attenuation, fFlags, pitch);
}

// ---------------------------------------------------------------------------
// Install / remove the hook
// Called once from Initialize() in cdll_int.cpp after gEngfuncs is populated.
// ---------------------------------------------------------------------------

void Tinnitus_Hook()
{
    if (gEngfuncs.pEventAPI && !s_EV_PlaySound_Original)
    {
        s_EV_PlaySound_Original           = gEngfuncs.pEventAPI->EV_PlaySound;
        gEngfuncs.pEventAPI->EV_PlaySound = EV_PlaySound_Tinnitus;
    }
}

void Tinnitus_Unhook()
{
    if (s_EV_PlaySound_Original)
    {
        gEngfuncs.pEventAPI->EV_PlaySound = s_EV_PlaySound_Original;
        s_EV_PlaySound_Original           = nullptr;
    }
}

// ---------------------------------------------------------------------------
// Internal: play one instance of the buzz
// ---------------------------------------------------------------------------

static void Tinnitus_PlayOnce()
{
    struct cl_entity_s *local = gEngfuncs.GetLocalPlayer();
    if (local && s_EV_PlaySound_Original)
    {
        s_EV_PlaySound_Original(
            local->index,
            local->origin,
            CHAN_STATIC,
            TINNITUS_SOUND,
            TINNITUS_VOLUME,
            ATTN_NONE,  // no positional attenuation - it's "inside your head"
            0,
            PITCH_NORM);
    }
}

// ---------------------------------------------------------------------------
// Frame tick - called every frame from HUD_Frame in cdll_int.cpp.
// Replays the sound every TINNITUS_DURATION seconds while active.
// ---------------------------------------------------------------------------

void Tinnitus_Frame(double time)
{
    if (!g_bTinnitusActive)
        return;

    // Use absolute client time, not the frame-delta passed in from HUD_Frame
    double clientTime = gEngfuncs.GetClientTime();

    if (clientTime >= s_flNextPlay)
    {
        Tinnitus_PlayOnce();
        s_flNextPlay = clientTime + TINNITUS_DURATION;
    }
}

// ---------------------------------------------------------------------------
// Client commands - triggered by server via CLIENT_COMMAND
// ---------------------------------------------------------------------------

static void Cmd_TinnitusStart()
{
    if (g_bTinnitusActive)
        return;

    g_bTinnitusActive = true;
    s_flNextPlay      = 0.0;  // force immediate play on the next frame
}

static void Cmd_TinnitusStop()
{
    if (!g_bTinnitusActive)
        return;

    g_bTinnitusActive = false;
    s_flNextPlay      = 0.0;

    struct cl_entity_s *local = gEngfuncs.GetLocalPlayer();
    if (local && gEngfuncs.pEventAPI)
    {
        gEngfuncs.pEventAPI->EV_StopSound(
            local->index,
            CHAN_STATIC,
            TINNITUS_SOUND);
    }
}

// ---------------------------------------------------------------------------
// Register client commands
// Called from HUD_Init() in cdll_int.cpp.
// ---------------------------------------------------------------------------

void Tinnitus_RegisterCommands()
{
    gEngfuncs.pfnAddCommand("tinnitus_start", Cmd_TinnitusStart);
    gEngfuncs.pfnAddCommand("tinnitus_stop",  Cmd_TinnitusStop);
}
