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

#if HANDMADE_INTERNAL
struct debug_read_file_result
{
    void *Contents;
    uint32 ContentsSize;
};

debug_read_file_result DEBUGPlatformReadEntireFile(char *FileName);
void DEBUGPlatformFreeFileMemory(void *Memory);

bool32 DEBUGPlatformWriteEntireFile(char *FileName, uint32 MemorySize, void *Memory);
#endif

#if HANDMADE_SLOW
#define Assert(Expression) if (!(Expression)) {*(int*)0 = 0;}
#else
#define Assert(Expression)
#endif

#define ArrayCount(Array) (sizeof(Array)/sizeof((Array)[0]))

inline uint32 SafeTruncateUInt64(uint64 Value)
{
    Assert(Value <= 0xFFFFFFFF);
    return((uint32)Value);
}

#define Kilobytes(Value) ((Value)*1024LL)
#define Megabytes(Value) (Kilobytes(Value)*1024LL)
#define Gigabytes(Value) (Megabytes(Value)*1024LL)
#define Terabytes(Value) (Gigabytes(Value)*1024LL)

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
    bool32 IsConnected;
    //TODO(voven): in future use vectors
    real32 StickAverageX;
    real32 StickAverageY;
    
    union
    {
        game_button_state Buttons[12];
        struct
        {
            game_button_state MoveUp;
            game_button_state MoveDown;
            game_button_state MoveLeft;
            game_button_state MoveRight;
            
            game_button_state A;
            game_button_state B;
            game_button_state X;
            game_button_state Y;
            
            game_button_state LeftShoulder;
            game_button_state RightShoulder;
            
            game_button_state Back;
            game_button_state Start;
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
    //NOTE(voven): 4 gamepads and 1 keyboard
    game_controller_input Controllers[5];
};

inline game_controller_input *GetController(game_input *Input, int ControllerIndex)
{
    Assert(ControllerIndex < ArrayCount(Input->Controllers));
    game_controller_input *Result = &Input->Controllers[ControllerIndex];
    return(Result);
}

internal void GameUpdateAndRender(game_memory* Memory, game_input* Input, game_offscreen_buffer* Buffer);
internal void GameGetSoundSamples(game_memory* Memory, game_sound_output_buffer* SoundBuffer);

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