#include "handmade.h"

void GameOutputSound(game_state *GameState, game_sound_output_buffer* SoundBuffer)
{
    int16 ToneVolume = 3000;
    int WavePeriod = SoundBuffer->SamplesPerSecond / GameState->ToneHz;
    
    int16 *SampleOut = SoundBuffer->Samples;
    
    for(int SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; ++SampleIndex)
    {
        real32 SineValue = sinf(GameState->tSine);
        int16 SampleValue = (int16)(SineValue * ToneVolume);
        *SampleOut++ = SampleValue;
        *SampleOut++ = SampleValue;
        
        GameState->tSine += 2.0f * (real32)Pi32 * (1.0f / (real32)WavePeriod);
        GameState->tSine = (real32)fmod(GameState->tSine, 2.0f * (real32)Pi32);
    }
}

void RenderWeirdGradeint(game_offscreen_buffer *Buffer, int XOffset, int YOffset) 
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


extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    Assert(sizeof(game_state) <= Memory->PermanentStorageSize);
    Assert((&Input->Controllers[0].Start - &Input->Controllers[0].Buttons[0]) == (ArrayCount(Input->Controllers[0].Buttons)) - 1);
    
    game_state *GameState = (game_state*)Memory->PermanentStorage;
    
    if (!Memory->IsInitialized)
    {
        char *FileName = __FILE__;
        
        debug_read_file_result File = Memory->DEBUGPlatformReadEntireFile(FileName);
        
        if (File.Contents)
        {
            //NOTE(voven): work directory in build folder
            Memory->DEBUGPlatformWriteEntireFile("test.out", File.ContentsSize, File.Contents);
            Memory->DEBUGPlatformFreeFileMemory(File.Contents);
        }
        
        GameState->ToneHz = 256;
        GameState->tSine = 0.0f;
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
    
    RenderWeirdGradeint(Buffer, GameState->BlueOffset, GameState->GreenOffset);
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
    game_state *GameState = (game_state*)Memory->PermanentStorage;
    GameOutputSound(GameState, SoundBuffer);
}