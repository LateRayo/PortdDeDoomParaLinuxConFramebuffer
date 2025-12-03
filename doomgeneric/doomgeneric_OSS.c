// doomgeneric_OSS.c
// OSS (Open Sound System) backend for DoomGeneric

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>

#include "i_sound.h"
#include "i_video.h"
#include "m_argv.h"

// Parámetros OSS
#define OSS_DEVICE    "/dev/dsp"
#define OSS_RATE      11025        // Hz
#define OSS_FORMAT    AFMT_S16_LE  // 16-bit little endian
#define OSS_CHANNELS  1            // mono

static int oss_fd = -1;

// Inicializa OSS
static boolean I_OSS_InitSound(boolean use_sfx_prefix)
{
    (void)use_sfx_prefix;

    oss_fd = open(OSS_DEVICE, O_WRONLY);
    if (oss_fd < 0) {
        perror("I_InitSound: open");
        return false;
    }

    { // formato
        int fmt = OSS_FORMAT;
        if (ioctl(oss_fd, SNDCTL_DSP_SETFMT, &fmt) < 0)
            perror("I_InitSound: SETFMT");
        if (fmt != OSS_FORMAT)
            fprintf(stderr, "OSS: formato inesperado %d\n", fmt);
    }

    { // canales
        int ch = OSS_CHANNELS;
        if (ioctl(oss_fd, SNDCTL_DSP_CHANNELS, &ch) < 0)
            perror("I_InitSound: CHANNELS");
        if (ch != OSS_CHANNELS)
            fprintf(stderr, "OSS: canales inesperados %d\n", ch);
    }

    { // tasa de muestreo
        int rate = OSS_RATE;
        if (ioctl(oss_fd, SNDCTL_DSP_SPEED, &rate) < 0)
            perror("I_InitSound: SPEED");
        if (rate != OSS_RATE)
            fprintf(stderr, "OSS: rate inesperada %d\n", rate);
    }

    return true;
}

// Cierra OSS
static void I_OSS_ShutdownSound(void)
{
    if (oss_fd >= 0) {
        close(oss_fd);
        oss_fd = -1;
    }
}

// Envía el buffer PCM a OSS
static void I_OSS_UpdateSound(void)
{
     // Supón que cada tic el motor llena DG_SoundBuffer
}

static void I_OSS_SubmitSound(void* buffer, int size)
{
    if (oss_fd < 0 || !buffer || size <= 0) return;
    ssize_t w = write(oss_fd, buffer, size);
    if (w < 0) perror("I_SubmitSound: write");
}

// No implementamos por canal, dejamos stubs:
static int I_OSS_GetSfxLumpNum(sfxinfo_t* sfxinfo)   { return S_GetSfxLumpNum(sfxinfo); }
static int I_OSS_StartSound(sfxinfo_t* sfxinfo, int channel, int vol, int sep) {
    // Para simplificar, el engine llamará SubmitSound en Update.
    return 0;
}
static void I_OSS_StopSound(int channel)             {}
static boolean I_OSS_SoundIsPlaying(int channel)     { return false; }
static void I_OSS_UpdateSoundParams(int channel,int vol,int sep) {}
static void I_OSS_PrecacheSounds(sfxinfo_t* sfx,int num) {}

// Lista de dispositivos OSS
static snddevice_t oss_devs[] = { SNDDEVICE_SB };

// Módulo de sonido OSS
sound_module_t DG_sound_module = {
    oss_devs,                // dispositivos soportados
    1,                       // número de dispositivos
    I_OSS_InitSound,         // Init(use_sfx_prefix)
    I_OSS_ShutdownSound,     // Shutdown()
    I_OSS_UpdateSound,       // Update()
    I_GetSfxLumpNum,         // GetSfxLumpNum(sfxinfo_t*)
    I_OSS_StartSound,        // StartSound(sfxinfo, channel, vol, sep)
    I_OSS_StopSound,         // StopSound(channel)
    I_OSS_SoundIsPlaying,    // SoundIsPlaying(channel)
    I_OSS_UpdateSoundParams, // UpdateSoundParams(channel, vol, sep)
    I_OSS_PrecacheSounds     // CacheSounds(sfxinfo_t*, int)
};