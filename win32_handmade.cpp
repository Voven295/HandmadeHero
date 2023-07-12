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
#include <xinput.h>
#include <dsound.h>

struct win32_offscreen_buffer
{
    BITMAPINFO Info;
    void* Memory;
    int Width;
    int Height;
    int Pitch;
};

global_variable LPDIRECTSOUNDBUFFER GlobalSoundBuffer;
global_variable win32_offscreen_buffer GlobalBackBuffer;
global_variable bool32 GlobalRunning;

struct win32_window_dimension {
    int Width;
    int Height;
};

struct win32_sound_output
{
    int32 SamplesPerSecond;
    int ToneHz;
    int16 ToneVolume;
    uint32 RunningSampleIndex;
    int WavePeriod;
    int BytesPerSample;
    int SecondaryBufferSize;
    int LatencySampleCount;
    real32 tSine;
};

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

internal void
Win32InitDSound(HWND Window, int32 SamplesPerSecond, int32 BufferSize) {
    HMODULE Library = LoadLibraryA("dsound.dll");
    if(Library) {
        direct_sound_create *DirectSoundCreate = (direct_sound_create *)GetProcAddress(Library, "DirectSoundCreate");
        
        LPDIRECTSOUND DirectSound;
        
        if(SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0))) {
            if(SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY))) {
                OutputDebugStringA("Set cooperative level ok\n");
            } else {
                // TODO: logging
            }
            
            WAVEFORMATEX WaveFormat  = {};
            WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
            WaveFormat.nChannels = 2;
            WaveFormat.nSamplesPerSec = SamplesPerSecond;
            WaveFormat.wBitsPerSample = 16;
            WaveFormat.nBlockAlign = WaveFormat.nChannels * WaveFormat.wBitsPerSample / 8;
            WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;
            
            {
                DSBUFFERDESC BufferDesc = {};
                BufferDesc.dwSize = sizeof(BufferDesc);
                BufferDesc.dwFlags = DSBCAPS_PRIMARYBUFFER;
                LPDIRECTSOUNDBUFFER  PrimaryBuffer;
                if(SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDesc, &PrimaryBuffer, 0))) {
                    OutputDebugStringA("Create primary buffer ok\n");
                    if(SUCCEEDED(PrimaryBuffer->SetFormat(&WaveFormat))) {
                        OutputDebugStringA("Primary buffer set format ok\n");
                    } else {
                        // TODO: logging
                    }
                }
            }
            
            {
                DSBUFFERDESC BufferDesc = {};
                BufferDesc.dwSize = sizeof(BufferDesc);
                BufferDesc.dwFlags = 0;
                BufferDesc.dwBufferBytes = BufferSize;
                BufferDesc.lpwfxFormat = &WaveFormat;
                if(SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDesc, &GlobalSoundBuffer, 0))) {
                    OutputDebugStringA("Secondary buffer created\n");
                } else {
                    // TODO: logging
                }
            }
            
        } else {
            // TODO: logging
        }
    } else {
        // TODO: logging
    }
}

internal void Win32ClearSoundBuffer(win32_sound_output *SoundOutput)
{
    void *Region1;
    DWORD Region1Size;
    void *Region2;
    DWORD Region2Size;
    
    if(SUCCEEDED(GlobalSoundBuffer->Lock(0, SoundOutput->SecondaryBufferSize,
                                         &Region1, &Region1Size,
                                         &Region2, &Region2Size,
                                         0))) 
    {
        uint8 *DestSample = (uint8*)Region1;
        
        for(DWORD ByteIndex = 0; ByteIndex < Region1Size; ++ByteIndex) {
            *DestSample++ = 0;
        }
        
        DestSample = (uint8*)Region2;
        
        for(DWORD ByteIndex = 0; ByteIndex < Region2Size; ++ByteIndex) {
            *DestSample++ = 0;
        }
        
        GlobalSoundBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
    }
}

internal void
Win32FillSoundBuffer(win32_sound_output *SoundOutput, DWORD LockOffset, DWORD BytesToLock, game_sound_output_buffer *SourceBuffer) {
    void *Region1;
    DWORD Region1Size;
    void *Region2;
    DWORD Region2Size;
    
    if(SUCCEEDED(GlobalSoundBuffer->Lock(LockOffset,
                                         BytesToLock,
                                         &Region1, &Region1Size,
                                         &Region2, &Region2Size,
                                         0))) 
    {
        DWORD Region1SampleCount = Region1Size / SoundOutput->BytesPerSample;
        
        int16 *DestSample = (int16 *)Region1;
        int16 *SourceSample = SourceBuffer->Samples;
        
        for(int i = 0; i < Region1SampleCount; i++) {
            *DestSample++ = *SourceSample++;
            *DestSample++ = *SourceSample++;
            SoundOutput->RunningSampleIndex++;
        }
        
        
        DWORD Region2SampleCount = Region2Size / SoundOutput->BytesPerSample;
        DestSample = (int16 *)Region2;
        for(int i = 0; i < Region2SampleCount; i++) {
            *DestSample++ = *SourceSample++;
            *DestSample++ = *SourceSample++;
            SoundOutput->RunningSampleIndex++;
        }
        
        GlobalSoundBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
    }
}


internal win32_window_dimension
Win32GetWindowDimension(HWND Window) {
    win32_window_dimension Result;
    RECT ClientRect;
    GetClientRect(Window, &ClientRect);
    Result.Width = ClientRect.right - ClientRect.left;
    Result.Height = ClientRect.bottom - ClientRect.top;
    return Result;
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
    Buffer->Info.bmiHeader.biBitCount = Buffer->Pitch * 8;
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
        
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            uint32 VKCode = WParam;
            bool32 WasDown = ((LParam & (1 << 30)) != 0);
            bool32 IsDown = ((LParam & (1 << 31)) == 0);
            
            if (IsDown != WasDown)
            {
                if (VKCode == 'E' || VKCode == VK_UP)
                {
                    OutputDebugStringA("E\n");
                }
                else if (VKCode == 'S' || VKCode == VK_LEFT)
                {
                    OutputDebugStringA("S\n");
                }
                else if (VKCode == 'D' || VKCode == VK_DOWN)
                {
                    OutputDebugStringA("D\n");
                }
                else if (VKCode == 'F' || VKCode == VK_RIGHT)
                {
                    OutputDebugStringA("F\n");
                }
                else if (VKCode == 'W')
                {
                    OutputDebugStringA("W\n");
                }
                else if (VKCode == 'R')
                {
                    OutputDebugStringA("R\n");
                }
                else if (VKCode == VK_ESCAPE)
                {
                    OutputDebugStringA("ESCAPE: ");
                    if (IsDown)
                    {
                        OutputDebugStringA("IsDown ");
                    }
                    if (WasDown)
                    {
                        OutputDebugStringA("WasDown");
                    }
                    OutputDebugStringA("\n");
                }
                else if (VKCode == VK_SPACE)
                {
                    OutputDebugStringA("SPACE\n");
                }
                bool32  AltKeyWasDown = ((LParam & (1 << 29)) != 0);
                if ((VKCode == VK_F4) && AltKeyWasDown)
                {
                    GlobalRunning = false;
                }
            }
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

int CALLBACK WinMain(HINSTANCE Instance, HINSTANCE PrevInstance,
                     LPSTR CommandLine, int ShowCode)
{
    LARGE_INTEGER PerfCountFrequencyResult;
    QueryPerformanceFrequency(&PerfCountFrequencyResult);
    int64 PerfCountFrequency = PerfCountFrequencyResult.QuadPart;
    
    Win32LoadXInput();
    
    WNDCLASSA WindowClass = {};
    
    Win32ResizeDIBSection(&GlobalBackBuffer, 1280, 720);
    
    WindowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    WindowClass.lpfnWndProc = Win32WindowProc;
    WindowClass.hInstance = Instance;
    //WindowClass.hIcon;
    WindowClass.lpszClassName = "HandmadeheroWindowClass";
    
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
            
            int XOffset = 0;
            int YOffset = 0;
            
            win32_sound_output SoundOutput = {};
            
            SoundOutput.SamplesPerSecond = 48000;
            SoundOutput.ToneHz = 256;
            SoundOutput.ToneVolume = 3000;
            SoundOutput.WavePeriod = SoundOutput.SamplesPerSecond / SoundOutput.ToneHz;
            SoundOutput.BytesPerSample = sizeof(int16)*2;
            SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;
            SoundOutput.LatencySampleCount = SoundOutput.SamplesPerSecond / 15;
            
            Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
            Win32ClearSoundBuffer(&SoundOutput);
            GlobalSoundBuffer->Play(0, 0, DSBPLAY_LOOPING);
            
            GlobalRunning = true;
            
            LARGE_INTEGER LastCounter;
            QueryPerformanceCounter(&LastCounter);
            
            uint64 LastCycleCount = __rdtsc();
            
            while(GlobalRunning)
            {
                MSG Message;
                while (PeekMessage(&Message, 0,0,0, PM_REMOVE))
                {
                    if (Message.message == WM_QUIT)
                    {
                        GlobalRunning = false;
                    }
                    
                    TranslateMessage(&Message);
                    DispatchMessageA(&Message);
                }
                
                
                for (DWORD ControllerIndex = 0; ControllerIndex < XUSER_MAX_COUNT; ++ControllerIndex)
                {
                    XINPUT_STATE ControllerState;
                    
                    if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
                    {
                        // controller is connected.
                        XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;
                        
                        bool32 Up = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
                        bool32 Down = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
                        bool32 Left = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
                        bool32 Right = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
                        bool32 Start = (Pad->wButtons & XINPUT_GAMEPAD_START);
                        bool32 Back = (Pad->wButtons & XINPUT_GAMEPAD_BACK);
                        bool32 LeftShoulder = (Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
                        bool32 RightShoulder = (Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
                        bool32 AButton = (Pad->wButtons & XINPUT_GAMEPAD_A);
                        bool32 BButton = (Pad->wButtons & XINPUT_GAMEPAD_B);
                        bool32 XButton = (Pad->wButtons & XINPUT_GAMEPAD_X);
                        bool32 YButton = (Pad->wButtons & XINPUT_GAMEPAD_Y);
                        
                        int16 StickX = Pad->sThumbLX;
                        int16 StickY = Pad->sThumbLY;
                        
                        XOffset += StickX / 12;
                        YOffset += StickY / 12;
                        
                        SoundOutput.ToneHz = 512 + (int)(256.0f*((real32)StickY / 30000.0f));
                        SoundOutput.WavePeriod = SoundOutput.SamplesPerSecond/SoundOutput.ToneHz;
                    }
                }
                
                
                bool32 SoundIsValid = false;
                DWORD ByteToLock;
                DWORD TargetCursor;
                DWORD BytesToWrite;
                DWORD PlayCursor;
                DWORD WriteCursor;
                
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
                
                int16 Samples[48000 * 2];
                game_sound_output_buffer SoundBuffer = {};
                SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
                SoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
                SoundBuffer.Samples = Samples;
                
                game_offscreen_buffer Buffer = {};
                Buffer.Memory = GlobalBackBuffer.Memory;
                Buffer.Width = GlobalBackBuffer.Width;
                Buffer.Height = GlobalBackBuffer.Height;
                Buffer.BytesPerPixel = GlobalBackBuffer.Pitch;
                
                GameUpdateAndRender(&Buffer, XOffset, YOffset, &SoundBuffer, SoundOutput.ToneHz);
                
                // BUG: Probably player cursor out of buffer when pull out a window 
                if (SoundIsValid)
                {
                    Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);
                }
                
                win32_window_dimension Dimension = Win32GetWindowDimension(Window);
                Win32DisplayBufferInWindow(&GlobalBackBuffer, DeviceContext, Dimension.Width, Dimension.Height);
                //ReleaseDC(Window, DeviceContext);
                
                ++XOffset;
                
                uint64 EndCycleCount = __rdtsc();
                
                LARGE_INTEGER EndCounter;
                QueryPerformanceCounter(&EndCounter);
                
                uint64 CyclesElapsed = EndCycleCount - LastCycleCount;
                
                int64 CounterElapsed = EndCounter.QuadPart - LastCounter.QuadPart;
                real32 MillisecondsPerFrame = ((real32)CounterElapsed * 1000.0f) / (real32)PerfCountFrequency;
                real32 FPS = (real32)PerfCountFrequency / (real32)CounterElapsed;
                real32 MCPF = (real32)CyclesElapsed / (1000.0f * 1000.0f);
#if 0
                char Buffer[256];
                sprintf(Buffer, "%0.02fms/f %0.02ff/s, %0.02fmc/f \n", MillisecondsPerFrame, FPS, MCPF);
                OutputDebugStringA(Buffer);
#endif
                
                LastCounter = EndCounter;
                LastCycleCount = EndCycleCount;
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


