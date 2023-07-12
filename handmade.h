#if !defined(HANDMADE_H)
// NOTE(voven): we need FOUR things: timing, controller/keyboadr input, bitmap buffer, sound buffer 

struct game_sound_output_buffer
{
    int SamplesPerSecond;
    int SampleCount;
    int16* Samples;
};

struct game_offscreen_buffer 
{
    void *Memory;
    int BytesPerPixel;
    int Width;
    int Height;
};

internal void GameUpdateAndRender(game_offscreen_buffer* Buffer, int XOffset, int YOffset, game_sound_output_buffer* SoundBuffer, int ToneHz);
#define HANDMADE_H
#endif