/*
     TODO(voven): 

  - Saved game location
- Getting a handle to our own executable file
- Assets loading path
- Threading (launch a thread)
- Raw input 
- Sleep/TimeBeginPeriod
 - ClipCursor() (multimonitor support)
- Fullscreen support
- WM_SETCURSOR (control cursor visibility)
- QuaryCancelAutoplay
- WM_ACTIVATEAPP (for when we are not active application)
- Blit speed improvements (BitBlit)
- Hardware acceleration (OpenGL or Direct3D or both???)
- GetKeyboardLayout (for French keyboards, international WASD support)
*/

#include <math.h>
#include <stdint.h>

#define Pi32 3.14159265359

#define local_persist static
#define global_variable static
#define internal static

typedef uint8_t uint8;
typedef uint32_t uint32;
typedef uint64_t uint64;
typedef uint16_t uint16;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef int32 bool32;

typedef float real32;
typedef double real64;

#include "handmade.cpp"

#include <windows.h>
#include <malloc.h>
#include <xinput.h>
#include <dsound.h>

#include "win32_handmade.h"

global_variable LPDIRECTSOUNDBUFFER GlobalSoundBuffer;
global_variable win32_offscreen_buffer GlobalBackBuffer;
global_variable bool32 GlobalRunning;
global_variable int64 GlobalPerfCountFrequency;

// NOTE(casey): XInputGetState
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)

typedef X_INPUT_GET_STATE(x_input_get_state);

X_INPUT_GET_STATE(XInputGetStateStub)
{
    return (ERROR_DEVICE_NOT_CONNECTED);
}

global_variable x_input_get_state* XInputGetState_ = XInputGetStateStub;

#define XInputGetState XInputGetState_

// NOTE(casey): XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)

typedef X_INPUT_SET_STATE(x_input_set_state);

X_INPUT_SET_STATE(XInputSetStateStub)
{
    return (ERROR_DEVICE_NOT_CONNECTED);
}

global_variable x_input_set_state* XInputSetState_ = XInputSetStateStub;

// NOTE(voven): direct sound.
#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter);
typedef DIRECT_SOUND_CREATE(direct_sound_create);

internal void Win32LoadXInput(void)
{
    HMODULE XInputLibrary = LoadLibrary("xinput1_4.dll");
    
    if (!XInputLibrary)
    {
        XInputLibrary = LoadLibrary("xinput9_1_0.dll");
    }
    
    if (!XInputLibrary)
    {
        XInputLibrary = LoadLibrary("xinput1_3.dll");
    }
    
    if (XInputLibrary)
    {
        XInputGetState_ = (x_input_get_state*)GetProcAddress(XInputLibrary, "XInputGetState");
        XInputSetState_ = (x_input_set_state*)GetProcAddress(XInputLibrary, "XInputSetState");
    }
}

internal debug_read_file_result DEBUGPlatformReadEntireFile(char *FileName)
{
    debug_read_file_result Result = {};
    
    HANDLE FileHandle = CreateFileA(FileName, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (FileHandle != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER FileSize;
        if (GetFileSizeEx(FileHandle, &FileSize))
        {
            uint32 FileSize32 = SafeTruncateUInt64(FileSize.QuadPart);
            Result.Contents = VirtualAlloc(0, FileSize32, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
            if (Result.Contents)
            {
                DWORD BytesRead = 0;
                
                if (ReadFile(FileHandle, Result.Contents, (DWORD)FileSize.QuadPart, &BytesRead, 0) && FileSize32 == BytesRead)
                {
                    Result.ContentsSize = FileSize32;
                }
                else
                {
                    DEBUGPlatformFreeFileMemory(Result.Contents);
                    Result.Contents = 0; 
                }
            }
            else 
            {
                
            }
        }
        else 
        {
            
        }
    }
    else 
    {
        
    }
    
    CloseHandle(FileHandle);
    
    return(Result);
}

internal void DEBUGPlatformFreeFileMemory(void *Memory)
{
    if (Memory)
    {
        VirtualFree(Memory, 0, MEM_RELEASE);
    }
}

internal bool32 DEBUGPlatformWriteEntireFile(char *FileName, uint32 MemorySize, void *Memory)
{
    bool32 Result = false;
    
    HANDLE FileHandle = CreateFileA(FileName, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
    if (FileHandle != INVALID_HANDLE_VALUE)
    {
        DWORD BytesWritten = 0;
        
        if (WriteFile(FileHandle, Memory, MemorySize, &BytesWritten, 0))
        {
            Result = BytesWritten == MemorySize;
        }
        else
        {
            
        }
    }
    else 
    {
        
    }
    
    CloseHandle(FileHandle);
    
    return(Result);
}

internal void Win32InitDirectSound(HWND Window, int32 SamplesPerSecond, int32 BufferSize)
{
    HMODULE DirectSoundLibrary = LoadLibraryA("dsound.dll");
    if (DirectSoundLibrary)
    {
        direct_sound_create *DirectSoundCreate = (direct_sound_create*)
            GetProcAddress(DirectSoundLibrary, "DirectSoundCreate");
        
        LPDIRECTSOUND DirectSound;
        if (DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
        {
            WAVEFORMATEX WaveFormat = {};
            WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
            WaveFormat.nChannels = 2;
            WaveFormat.nSamplesPerSec = SamplesPerSecond;
            WaveFormat.wBitsPerSample = 16;
            WaveFormat.nBlockAlign = (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8;
            WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;
            WaveFormat.cbSize = 0;
            
            if (SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY)))
            {
                DSBUFFERDESC BufferDescription = {};
                BufferDescription.dwSize = sizeof(BufferDescription);
                BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;
                
                LPDIRECTSOUNDBUFFER PrimaryBuffer;
                // NOTE(voven): first buffer need only for set WaveFormat in our soundcard
                if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0)))
                {
                    
                    if (SUCCEEDED(PrimaryBuffer->SetFormat(&WaveFormat)))
                    {
                    }
                }
            }
            
            // NOTE(voven): secondarybuffer represents the final buffer with which we will interact in the     // NOTE(voven): future to play the sound
            DSBUFFERDESC BufferDescription = {};
            BufferDescription.dwSize = sizeof(BufferDescription);
            BufferDescription.dwFlags = 0;
            BufferDescription.dwBufferBytes = BufferSize;
            BufferDescription.lpwfxFormat = &WaveFormat;
            
            if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &GlobalSoundBuffer, 0)))
            {
                
            }
        }
    }
}

internal void Win32ClearSoundBuffer(win32_sound_output *SoundOutput)
{
    VOID *Region1;
    DWORD Region1Size;
    VOID *Region2;
    DWORD Region2Size;
    
    if(SUCCEEDED(GlobalSoundBuffer->Lock(0, SoundOutput->SecondaryBufferSize,
                                         &Region1, &Region1Size,
                                         &Region2, &Region2Size,
                                         0)))
    {
        uint8 *DestSample = (uint8*) Region1;
        
        for(DWORD ByteIndex = 0; ByteIndex < Region1Size; ++ByteIndex)
        {
            *DestSample++ = 0;
        }
        
        DestSample = (uint8*) Region2;
        
        for(DWORD ByteIndex = 0; ByteIndex < Region2Size; ++ByteIndex)
        {
            *DestSample++ = 0;
        }
        
        GlobalSoundBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
    }
}

internal void Win32FillSoundBuffer(win32_sound_output *SoundOutput, DWORD BytesToLock, DWORD BytesToWrite,
                                   game_sound_output_buffer *SourceBuffer)
{
    VOID *Region1;
    VOID *Region2;
    DWORD Region1Size;
    DWORD Region2Size;
    
    if(SUCCEEDED(GlobalSoundBuffer->Lock(BytesToLock,
                                         BytesToWrite,
                                         &Region1, &Region1Size,
                                         &Region2, &Region2Size,
                                         0))) 
    {
        int Region1SampleCount = Region1Size / SoundOutput->BytesPerSample;
        
        int16 *DestSample = (int16 *)Region1;
        int16 *SourceSample = SourceBuffer->Samples;
        
        for(int i = 0; i < Region1SampleCount; i++) {
            *DestSample++ = *SourceSample++;
            *DestSample++ = *SourceSample++;
            ++SoundOutput->RunningSampleIndex;
        }
        
        
        int Region2SampleCount = Region2Size / SoundOutput->BytesPerSample;
        DestSample = (int16 *)Region2;
        
        for(int i = 0; i < Region2SampleCount; i++) {
            *DestSample++ = *SourceSample++;
            *DestSample++ = *SourceSample++;
            ++SoundOutput->RunningSampleIndex;
        }
        
        GlobalSoundBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
    }
}

internal void Win32ProcessDigitalXInputButton(DWORD XInputButtonState, game_button_state* OldState, DWORD ButtonBit, game_button_state* NewState)
{
    NewState->EndedDown = (XInputButtonState & ButtonBit) == ButtonBit;
    NewState->HalfTransitionCount = (OldState->EndedDown != NewState->EndedDown) ? 1 : 0;
}

internal void Win32ProcessKeyboardMessage(game_button_state* NewState, bool32 IsDown)
{
    Assert(NewState->EndedDown != IsDown);
    NewState->EndedDown = IsDown;
    ++NewState->HalfTransitionCount;
}

internal real32 Win32ProcessXInputStickValue(short StickValue, short DeadZoneThreshold)
{
    real32 Result = 0;
    if (StickValue < -DeadZoneThreshold)
    {
        Result = (real32)((StickValue + DeadZoneThreshold) / (32768.0f - DeadZoneThreshold));
    }
    else if (StickValue > DeadZoneThreshold)
    {
        Result = (real32)((StickValue - DeadZoneThreshold) / (32767.0f - DeadZoneThreshold));
    }
    
    return(Result);
}

internal win32_window_dimension Win32GetWindowDimension(HWND Window)
{
    win32_window_dimension Result;
    
    RECT ClientRect;
    GetClientRect(Window, &ClientRect);
    Result.Width = ClientRect.right - ClientRect.left;
    Result.Height = ClientRect.bottom - ClientRect.top;
    
    return(Result);
}

internal void Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width, int Height) 
{
    if(Buffer->Memory)
    {
        VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
    }
    
    Buffer->Width = Width;
    Buffer->Height = Height;
    Buffer->Pitch = 4;
    
    Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
    Buffer->Info.bmiHeader.biWidth = Buffer->Width;
    Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
    Buffer->Info.bmiHeader.biPlanes = 1;
    Buffer->Info.bmiHeader.biBitCount = (WORD)Buffer->Pitch * 8;
    Buffer->Info.bmiHeader.biCompression = BI_RGB;
    
    int BitmapSize = Buffer->Width * Buffer->Height * Buffer->Pitch;
    
    Buffer->Memory = VirtualAlloc(0, BitmapSize, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
}

internal void Win32DisplayBufferInWindow(win32_offscreen_buffer* Buffer, HDC DeviceContext, 
                                         int WindowWidth, int WindowHeight)
{
    StretchDIBits(DeviceContext,
                  0, 0, WindowWidth, WindowHeight, 
                  0, 0, Buffer->Width, Buffer->Height, 
                  Buffer->Memory,
                  &Buffer->Info,
                  DIB_RGB_COLORS,
                  SRCCOPY);
}


internal void Win32ProcessPendingMessages(game_controller_input* KeyboardController)
{
    MSG Message;
    while (PeekMessage(&Message, 0,0,0, PM_REMOVE))
    {
        switch(Message.message)
        {
            case WM_QUIT:
            {
                GlobalRunning = false;
            }break;
            
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_KEYDOWN:
            case WM_KEYUP:
            {
                uint32 VKCode = (uint32)Message.wParam;
                bool32 WasDown = ((Message.lParam & (1 << 30)) != 0);
                bool32 IsDown = ((Message.lParam & (1 << 31)) == 0);
                
                if (IsDown != WasDown)
                {
                    if (VKCode == 'E' || VKCode == VK_UP)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->MoveUp, IsDown);
                    }
                    else if (VKCode == 'S' || VKCode == VK_LEFT)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->MoveLeft, IsDown);
                    }
                    else if (VKCode == 'D' || VKCode == VK_DOWN)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->MoveDown, IsDown);
                    }
                    else if (VKCode == 'F' || VKCode == VK_RIGHT)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->MoveRight, IsDown);
                    }
                    else if (VKCode == 'W')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->LeftShoulder, IsDown);
                    }
                    else if (VKCode == 'R')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->RightShoulder, IsDown);
                    }
                    else if (VKCode == VK_ESCAPE)
                    {
                        GlobalRunning = false;
                        Win32ProcessKeyboardMessage(&KeyboardController->Start, IsDown);
                    }
                    else if (VKCode == VK_SPACE)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->Back, IsDown);
                    }
                    bool32 AltKeyWasDown = ((Message.lParam & (1 << 29)) != 0);
                    if ((VKCode == VK_F4) && AltKeyWasDown)
                    {
                        GlobalRunning = false;
                    }
                }
            }break;
            
            default:
            {
                TranslateMessage(&Message);
                DispatchMessageA(&Message);
            }break;
        }
    }
}

LRESULT CALLBACK Win32WindowProc(HWND Window, UINT Message, WPARAM WParam,
                                 LPARAM LParam)
{
    //LRESULT is 64 bit long
    LRESULT Result = 0;
    
    switch (Message) {
        case WM_SIZE: {
            
        }break;
        
        case WM_DESTROY: {
            GlobalRunning = false;
        }break;
        
        case WM_CLOSE: {
            GlobalRunning = false;
        }break;
        case WM_ACTIVATEAPP: {
            OutputDebugStringA("WM_ACTIVATEAPP\n");
        }break;
        
        case WM_PAINT:
        {
            PAINTSTRUCT Paint;
            HDC DeviceContext = BeginPaint(Window, &Paint);
            int X = Paint.rcPaint.left;
            int Y = Paint.rcPaint.top;
            int Width = Paint.rcPaint.right - Paint.rcPaint.left;
            int Height = Paint.rcPaint.bottom - Paint.rcPaint.top;
            RECT ClientRect;
            GetClientRect(Window, &ClientRect);
            
            win32_window_dimension Dimension = Win32GetWindowDimension(Window);
            Win32DisplayBufferInWindow(&GlobalBackBuffer, DeviceContext, Dimension.Width, Dimension.Height);
            EndPaint(Window, &Paint);
        }break;
        
        default: {
            Result = DefWindowProcA(Window, Message, WParam, LParam);
        }break;
    }
    
    return Result;
    
}

inline LARGE_INTEGER Win32GetWallClock()
{
    LARGE_INTEGER Result;
    QueryPerformanceCounter(&Result);
    return(Result);
}

inline real32 Win32GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End)
{
    real32 Result = (real32)(End.QuadPart - Start.QuadPart) / (real32)GlobalPerfCountFrequency;
    return(Result);
}

int CALLBACK WinMain(HINSTANCE Instance, HINSTANCE PrevInstance,
                     LPSTR CommandLine, int ShowCode)
{
    LARGE_INTEGER PerfCountFrequencyResult;
    QueryPerformanceFrequency(&PerfCountFrequencyResult);
    GlobalPerfCountFrequency = PerfCountFrequencyResult.QuadPart;
    
    //NOTE(voven): Set the Windows Scheduler granularity to 1 ms
    //so that our sleep can be mo granular  
    UINT DesiredSchedulerMS = 1;
    bool32 SleepIsGranular = (timeBeginPeriod(DesiredSchedulerMS) == TIMERR_NOERROR);
    
    Win32LoadXInput();
    
    WNDCLASSA WindowClass = {};
    
    Win32ResizeDIBSection(&GlobalBackBuffer, 1280, 720);
    
    WindowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    WindowClass.lpfnWndProc = Win32WindowProc;
    WindowClass.hInstance = Instance;
    //WindowClass.hIcon;
    WindowClass.lpszClassName = "HandmadeheroWindowClass";
    
    int MonitorRefreshHz = 144;
    int GameUpdateHz = MonitorRefreshHz / 2;
    real32 TargetSecondsPerFrame = 1.0f / (real32)GameUpdateHz;
    
    if (RegisterClassA(&WindowClass))
    {
        HWND Window = CreateWindowExA
        (
         0, 
         WindowClass.lpszClassName, 
         "Handmade Hero", 
         WS_OVERLAPPEDWINDOW | WS_VISIBLE, 
         CW_USEDEFAULT, 
         CW_USEDEFAULT, 
         CW_USEDEFAULT, 
         CW_USEDEFAULT, 
         0, 
         0, 
         Instance, 
         0);
        
        if (Window)
        {
            HDC DeviceContext = GetDC(Window);
            
            win32_sound_output SoundOutput = {};
            
            SoundOutput.SamplesPerSecond = 48000;
            SoundOutput.BytesPerSample = sizeof(int16)*2;
            SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;
            SoundOutput.LatencySampleCount = SoundOutput.SamplesPerSecond / 15;
            
            Win32InitDirectSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
            Win32ClearSoundBuffer(&SoundOutput);
            GlobalSoundBuffer->Play(0, 0, DSBPLAY_LOOPING);
            
            GlobalRunning = true;
            
            int16* Samples = (int16*)VirtualAlloc(0, SoundOutput.SecondaryBufferSize, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
            
#if HANDMADE_INTERNAL
            LPVOID BaseAddress = (LPVOID)Terabytes(2);
#else
            LPVOID BaseAddress = 0;
#endif
            
            game_memory GameMemory = {};
            
            GameMemory.PermanentStorageSize = Megabytes(64);
            GameMemory.TransientStorageSize = Gigabytes(4);
            
            uint64 TotalSize = GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize;
            
            GameMemory.PermanentStorage = 
            (int16*)VirtualAlloc(BaseAddress, (size_t)TotalSize, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
            
            GameMemory.TransientStorage = (uint8*)GameMemory.PermanentStorage + GameMemory.PermanentStorageSize;
            
            if (Samples && GameMemory.PermanentStorage && GameMemory.TransientStorage)
            {
                game_input Input[2] = {};
                game_input* NewInput = &Input[0];
                game_input* OldInput = &Input[1];
                
                LARGE_INTEGER LastCounter = Win32GetWallClock();
                //NOTE(voven): only for profiling, not for test on user machine
                uint64 LastCycleCount = __rdtsc();
                
                while(GlobalRunning)
                {
                    game_controller_input *OldKeyboardController = GetController(OldInput, 0);
                    game_controller_input *NewKeyboardController = GetController(NewInput, 0);
                    
                    *NewKeyboardController = {};
                    NewKeyboardController->IsConnected = true;
                    
                    for(int ButtonIndex = 0; ButtonIndex < ArrayCount(NewKeyboardController->Buttons); ++ButtonIndex)
                    {
                        //NOTE(voven): this needs to holding buttons
                        NewKeyboardController->Buttons[ButtonIndex].EndedDown= OldKeyboardController->Buttons[ButtonIndex].EndedDown;
                    }
                    
                    Win32ProcessPendingMessages(NewKeyboardController);
                    
                    DWORD MaxControllerCount = XUSER_MAX_COUNT;
                    
                    if (MaxControllerCount > ArrayCount(NewInput->Controllers) - 1)
                    {
                        MaxControllerCount = ArrayCount(NewInput->Controllers) - 1;
                    }
                    
                    for (DWORD ControllerIndex = 0; ControllerIndex < MaxControllerCount - 1; ++ControllerIndex)
                    {
                        DWORD OurControllerIndex = ControllerIndex + 1;
                        game_controller_input* OldController = GetController(OldInput, OurControllerIndex);
                        game_controller_input* NewController = GetController(NewInput, OurControllerIndex);
                        
                        XINPUT_STATE ControllerState;
                        
                        if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
                        {
                            // controller is connected.
                            NewController->IsConnected = true;
                            XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;
                            NewController->IsAnalog = true;
                            
                            NewController->StickAverageX = Win32ProcessXInputStickValue(Pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                            NewController->StickAverageY = Win32ProcessXInputStickValue(Pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                            
                            if ((NewController->StickAverageX != 0.0f) || 
                                (NewController->StickAverageY != 0.0f))
                            {
                                NewController->IsAnalog = true;
                            }
                            
                            //NOTE(voven): dpad
                            if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP)
                            {
                                NewController->StickAverageY = 1.0f;
                                NewController->IsAnalog = false;
                            }
                            
                            if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
                            {
                                NewController->StickAverageY = -1.0f;
                                NewController->IsAnalog = false;
                            }
                            
                            if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
                            {
                                NewController->StickAverageX = -1.0f;
                                NewController->IsAnalog = false;
                            }
                            
                            if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
                            {
                                NewController->StickAverageX = 1.0f;
                                NewController->IsAnalog = false;
                            }
                            
                            //NOTE(voven): i dont know why is this need
                            real32 Threshold = 0.5f;
                            Win32ProcessDigitalXInputButton((NewController->StickAverageX < -Threshold) ? 1 : 0, 
                                                            &OldController->MoveLeft, 1, &NewController->MoveLeft);
                            Win32ProcessDigitalXInputButton((NewController->StickAverageX > Threshold) ? 1 : 0, 
                                                            &OldController->MoveRight, 1, &NewController->MoveRight);
                            Win32ProcessDigitalXInputButton((NewController->StickAverageY < -Threshold) ? 1 : 0, 
                                                            &OldController->MoveDown, 1, &NewController->MoveDown);
                            Win32ProcessDigitalXInputButton((NewController->StickAverageY > Threshold) ? 1 : 0, 
                                                            &OldController->MoveUp, 1, &NewController->MoveUp);
                            
                            Win32ProcessDigitalXInputButton(Pad->wButtons, 
                                                            &OldController->A, XINPUT_GAMEPAD_A, &NewController->A);
                            Win32ProcessDigitalXInputButton(Pad->wButtons, 
                                                            &OldController->B, XINPUT_GAMEPAD_B, &NewController->B);
                            Win32ProcessDigitalXInputButton(Pad->wButtons, 
                                                            &OldController->X, XINPUT_GAMEPAD_X, &NewController->X);
                            Win32ProcessDigitalXInputButton(Pad->wButtons, 
                                                            &OldController->Y, XINPUT_GAMEPAD_Y, &NewController->Y);
                            Win32ProcessDigitalXInputButton(Pad->wButtons, 
                                                            &OldController->LeftShoulder, XINPUT_GAMEPAD_LEFT_SHOULDER, &NewController->LeftShoulder);
                            Win32ProcessDigitalXInputButton(Pad->wButtons, 
                                                            &OldController->RightShoulder, XINPUT_GAMEPAD_RIGHT_SHOULDER, &NewController->RightShoulder);
                            Win32ProcessDigitalXInputButton(Pad->wButtons, 
                                                            &OldController->Back, XINPUT_GAMEPAD_BACK, &NewController->Back);
                            Win32ProcessDigitalXInputButton(Pad->wButtons, 
                                                            &OldController->Start, XINPUT_GAMEPAD_START, &NewController->Start);
                        }
                        else
                        {
                            NewController->IsConnected = false;
                        }
                    }
                    
                    
                    bool32 SoundIsValid = false;
                    DWORD ByteToLock = 0;
                    DWORD TargetCursor = 0;
                    DWORD BytesToWrite = 0;
                    DWORD PlayCursor = 0;
                    DWORD WriteCursor = 0;
                    
                    if(SUCCEEDED(GlobalSoundBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor))) 
                    {
                        ByteToLock = SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample % SoundOutput.SecondaryBufferSize;
                        TargetCursor = (PlayCursor + SoundOutput.LatencySampleCount * SoundOutput.BytesPerSample) % SoundOutput.SecondaryBufferSize;
                        
                        if(ByteToLock > TargetCursor)
                            
                        {
                            BytesToWrite = SoundOutput.SecondaryBufferSize - ByteToLock + TargetCursor;
                        } 
                        else
                        {
                            BytesToWrite = TargetCursor - ByteToLock;
                        }
                        
                        SoundIsValid = true;
                    }
                    
                    game_sound_output_buffer SoundBuffer = {};
                    SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
                    SoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
                    SoundBuffer.Samples = Samples;
                    
                    game_offscreen_buffer Buffer = {};
                    Buffer.Memory = GlobalBackBuffer.Memory;
                    Buffer.Width = GlobalBackBuffer.Width;
                    Buffer.Height = GlobalBackBuffer.Height;
                    Buffer.BytesPerPixel = GlobalBackBuffer.Pitch;
                    
                    GameUpdateAndRender(&GameMemory, NewInput, &Buffer, &SoundBuffer);
                    
                    // BUG: Probably player cursor out of buffer when pull out a window 
                    if (SoundIsValid)
                    {
                        Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);
                    }
                    
                    //ReleaseDC(Window, DeviceContext);
                    
                    LARGE_INTEGER WorkCounter = Win32GetWallClock();
                    real32 WorkSecondsElapsed = Win32GetSecondsElapsed(LastCounter, WorkCounter);
                    real32 SecondsElapsedForFrame = WorkSecondsElapsed;
                    
                    if (SecondsElapsedForFrame < TargetSecondsPerFrame)
                    {
                        while(SecondsElapsedForFrame < TargetSecondsPerFrame)
                        {
                            if(SleepIsGranular)
                            {
                                DWORD SleepMS = (DWORD)(1000.0f * (TargetSecondsPerFrame - SecondsElapsedForFrame));
                                Sleep(SleepMS);
                            }
                            SecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());
                        }
                    }
                    else
                    {
                        //NOTE(voven): Missing framerate!
                        //NOTE(voven): Logging
                    }
                    
                    win32_window_dimension Dimension = Win32GetWindowDimension(Window);
                    Win32DisplayBufferInWindow(&GlobalBackBuffer, DeviceContext, Dimension.Width, Dimension.Height);
                    
                    //TODO(voven): Remove this in later
                    game_input *Temp = NewInput;
                    NewInput = OldInput;
                    OldInput = Temp;
                    
                    LARGE_INTEGER EndCounter = Win32GetWallClock();
                    real32 MillisecondsPerFrame = 1000.0f * Win32GetSecondsElapsed(LastCounter, EndCounter);
                    LastCounter = EndCounter;
                    
                    uint64 EndCycleCount = __rdtsc();
                    uint64 CyclesElapsed = EndCycleCount - LastCycleCount;
                    LastCycleCount = EndCycleCount;
                    
                    //real64 FPS = (real64)GlobalPerfCountFrequency / (real64)
                    real64 FPS = 0.0f;
                    real64 MCPF = (real64)CyclesElapsed / (1000.0f * 1000.0f);
                    
                    char FPSBuffer[256];
                    wsprintfA(FPSBuffer, "ms/f: %d\n", MillisecondsPerFrame);
                    OutputDebugStringA(FPSBuffer);
                    //_snprintf_s(FPSBuffer, sizeof(FPSBuffer), "%0.02fms/f %0.02ff/s, %0.02fmc/f \n", MillisecondsPerFrame, FPS, MCPF);
                }
            }
            else
            {
                
            }
        }
        else
        {
            
        }
    }
    else 
    {
        
    }
    
    return 0;
}


