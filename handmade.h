#if !defined(HANDMADE_H)
// NOTE(voven): we need FOUR things: timing, controller/keyboadr input, bitmap buffer, sound buffer 

/*
  HANDMADE_INTERNAL:
 0 - Public release
1 - Developer build

HANDMADE_SLOW:
0 - Not debuging
1 - Debugging
*/

#if HANDMADE_SLOW
#define Assert(Expression) if (!(Expression)) {*(int*)0 = 0;}
#else
#define Assert(Expression)
#endif

#define ArrayCount(Array) (sizeof(Array)/sizeof((Array)[0]))

#define Kilobytes(Value) ((Value)*1024)
#define Megabytes(Value) (Kilobytes(Value)*1024)
#define Gigabytes(Value) (Megabytes(Value)*1024)
#define Terabytes(Value) (Gigabytes(Value)*1024)

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

struct game_button_state
{
    int HalfTransitionCount;
    bool32 EndedDown;
};

struct game_controller_input
{
    bool32 IsAnalog;
    
    //TODO(voven): in future use vectors
    real32 StartX;
    real32 StartY;
    
    real32 MinX;
    real32 MinY;
    
    real32 MaxX;
    real32 MaxY;
    
    real32 EndX;
    real32 EndY;
    
    union
    {
        game_button_state Buttons[6];
        struct
        {
            game_button_state Up;
            game_button_state Down;
            game_button_state Left;
            game_button_state Right;
            game_button_state LeftShoulder;
            game_button_state RightShoulder;
        };
    };
};

struct game_memory
{
    bool32 IsInitialized;
    uint64 PermanentStorageSize;
    void *PermanentStorage;
    void *TransientStorage;
    uint64 TransientStorageSize;
};

struct game_input
{
    game_controller_input Controllers[4];
};

internal void GameUpdateAndRender(game_input* Input,game_offscreen_buffer* Buffer,game_sound_output_buffer* SoundBuffer);

//
//
//

struct game_state
{
    int ToneHz;
    int GreenOffset;
    int BlueOffset;
};

#define HANDMADE_H
#endif