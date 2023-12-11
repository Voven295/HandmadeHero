#include "handmade.h"

internal void GameOutputSound(game_sound_output_buffer* SoundBuffer, int ToneHz)
{
    local_persist real32 tSine;
    int16 ToneVolume = 3000;
    //int ToneHz = 256;
    int WavePeriod = SoundBuffer->SamplesPerSecond / ToneHz;
    
    int16 *SampleOut = SoundBuffer->Samples;
    
    for(int SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; ++SampleIndex)
    {
        real32 SineValue = sinf(tSine);
        int16 SampleValue = (int16)(SineValue * ToneVolume);
        *SampleOut++ = SampleValue;
        *SampleOut++ = SampleValue;
        
        tSine += 2.0f * (real32)Pi32 * (1.0f / (real32)WavePeriod);
    }
}

internal void RenderWeirdGradeint(game_offscreen_buffer *Buffer, int XOffset, int YOffset) 
{
    uint8 *Row = (uint8 *)Buffer->Memory;
    
    for(int Y = 0; Y < Buffer->Height; Y++) {
        uint32 *Pixel = (uint32 *)Row;
        
        for(int X = 0; X < Buffer->Width; X++) {
            uint8 Blue = (uint8)(X + XOffset);
            uint8 Green =(uint8)(Y + YOffset);
            // 0xXXRRGGBB
            *Pixel++ = (Green << 8) | Blue;
        }
        Row = Row + Buffer->Width * Buffer->BytesPerPixel;
    }
}


internal void GameUpdateAndRender(game_memory* Memory, game_input* Input, game_offscreen_buffer* Buffer, game_sound_output_buffer* SoundBuffer)
{
    Assert(sizeof(game_state) <= Memory->PermanentStorageSize);
    Assert((&Input->Controllers[0].Start - &Input->Controllers[0].Buttons[0]) == (ArrayCount(Input->Controllers[0].Buttons)) - 1);
    
    game_state *GameState = (game_state*)Memory->PermanentStorage;
    
    if (!Memory->IsInitialized)
    {
        char *FileName = __FILE__;
        
        debug_read_file_result File = DEBUGPlatformReadEntireFile(FileName);
        
        if (File.Contents)
        {
            //NOTE(voven): work directory in build folder
            DEBUGPlatformWriteEntireFile("test.out", File.ContentsSize, File.Contents);
            DEBUGPlatformFreeFileMemory(File.Contents);
        }
        
        
        GameState->ToneHz = 256;
        Memory->IsInitialized = true;
    }
    
    for(int ControllerIndex = 0; ControllerIndex < ArrayCount(Input->Controllers); ++ControllerIndex)
    {
        game_controller_input *Controller = GetController(Input, ControllerIndex);
        
        if (Controller->IsAnalog)
        {
            //NOTE(voven): analog - gamepad
            GameState->BlueOffset += (int)(4.0f*(Controller->StickAverageX));
            GameState->GreenOffset -= (int)(4.0f*(Controller->StickAverageY));
            GameState->ToneHz = 256 + (int)(128.0f*(Controller->StickAverageY));
        }
        else
        {
            if (Controller->MoveRight.EndedDown)
            {
                GameState->BlueOffset += 1;
            }
            
            if (Controller->MoveLeft.EndedDown)
            {
                GameState->BlueOffset -= 1;
            }
            
            if (Controller->MoveUp.EndedDown)
            {
                GameState->GreenOffset -= 1;
            }
            
            if (Controller->MoveDown.EndedDown)
            {
                GameState->GreenOffset += 1;
            }
        }
    }
    
    GameOutputSound(SoundBuffer, GameState->ToneHz);
    RenderWeirdGradeint(Buffer, GameState->BlueOffset, GameState->GreenOffset);
}