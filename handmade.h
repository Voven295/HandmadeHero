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
    void* Memory;
    int Width;
    int Height;
    int Pitch;
};

internal void GameUpdateAndRender(game_offscreen_buffer *Buffer, game_sound_output_buffer *SoundBuffer, int BlueOffset, int GreenOffset, int ToneHz);
#define HANDMADE_H
#endif