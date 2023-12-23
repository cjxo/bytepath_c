#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define COBJMACROS
#include <windows.h>
#include <timeapi.h>
#include <d3d11.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#undef far
#undef near

#include <time.h>
#include <math.h>
#include <stdio.h>
#include "bp_base.h"
#include "bp_base.c"

global u64 w32_ticks_per_second;

function void *
OS_ReserveMemory(u64 size_in_bytes)
{
    void *block = VirtualAlloc(0, size_in_bytes, MEM_RESERVE, PAGE_NOACCESS);
    return(block);
}

function b32
OS_CommitMemory(void *memory_to_commit, u64 size_in_bytes)
{
    b32 success = VirtualAlloc(memory_to_commit, size_in_bytes, MEM_COMMIT, PAGE_READWRITE) != null;
    return(success);
}

function b32
OS_DecommitMemory(void *memory_to_decommit, u64 size_in_bytes)
{
    b32 success = VirtualFree(memory_to_decommit, size_in_bytes, MEM_DECOMMIT) != FALSE;
    return(success);
}

function b32
OS_ReleaseMemory(void *memory_to_release)
{
    b32 success = VirtualFree(memory_to_release, 0, MEM_RELEASE) != FALSE;
    return(success);
}

function void
OS_Sleep(u64 milliseconds)
{
    Sleep((DWORD)milliseconds);
}

typedef enum Key_Code
{
    KeyCode_Escape,
    KeyCode_Total
} Key_Code;

typedef enum Input_Interact_Type
{
    InputInteract_Pressed = 0x1,
    InputInteract_Released = 0x2,
    InputInteract_Held = 0x4,
} Input_Interact_Type;

typedef enum Misc_Input_Flag
{
    InputFlag_Quit = 0x1,
} Misc_Input_Flag;

typedef struct OS_Input
{
    u32 key_states[KeyCode_Total];
    u8 misc_flags;
} OS_Input;

typedef struct W32_State
{
    HWND window;
    OS_Input input;
} W32_State;

global W32_State g_w32_state;

function Key_Code
W32_MapWParamToKeyCode(WPARAM wparam)
{
    Key_Code result;
    switch (wparam)
    {
        case VK_ESCAPE:
        {
            result = KeyCode_Escape;
        } break;
        
        default:
        {
            result = KeyCode_Total;
        } break;
    }
    
    return(result);
}

function b32
OS_KeyPressed(Key_Code key)
{
    b32 result = False;
    if (key < KeyCode_Total)
    {
        result = g_w32_state.input.key_states[key] & InputInteract_Pressed;
    }
    return(result);
}

function b32
OS_KeyReleased(u32 key)
{
    b32 result = False;
    if (key < KeyCode_Total)
    {
        result = g_w32_state.input.key_states[key] & InputInteract_Released;
    }
    return(result);
}

function b32
OS_KeyHeld(u32 key)
{
    b32 result = False;
    if (key < KeyCode_Total)
    {
        result = g_w32_state.input.key_states[key] & InputInteract_Held;
    }
    return(result);
}

function b32
OS_InputFlagGet(u8 input_flag)
{
    b32 result = g_w32_state.input.misc_flags & input_flag;
    return(result);
}

function void
OS_InputFlagSet(u8 input_flag, b32 enabled)
{
    if (enabled)
    {
        g_w32_state.input.misc_flags |= input_flag;
    }
    else
    {
        g_w32_state.input.misc_flags &= ~input_flag;
    }
}

inline u64
W32_GetTicks(void)
{
    LARGE_INTEGER result;
    QueryPerformanceCounter(&result);
    
    return(result.QuadPart);
}

inline f32
W32_SecondsBetweenTicksF32(u64 start, u64 end)
{
    const f32 seconds_per_ticks = 1.0f / (f32)w32_ticks_per_second;
    f32 delta_ticks = (f32)(end - start);
    f32 result = delta_ticks * seconds_per_ticks;
    return(result);
}

inline s32
W32_GetRefreshMonitorRefreshRate(HWND window_handle)
{
    HDC dc = GetDC(window_handle);
    s32 result = GetDeviceCaps(dc, VREFRESH);
    ReleaseDC(window_handle, dc);
    
    return(result);
}

function LRESULT __stdcall
W32_WindowProc(HWND window, UINT message,
               WPARAM wparam, LPARAM lparam)
{
    LRESULT result = 0;
    
    switch (message)
    {
        case WM_CLOSE:
        {
            DestroyWindow(window);
        } break;
        
        case WM_DESTROY:
        {
            PostQuitMessage(0);
        } break;
        
        default:
        {
            result = DefWindowProcA(window, message, wparam, lparam);
        } break;
    }
    
    return(result);
}

function void
W32_FillEvents(void)
{
    OS_Input *os_input = &(g_w32_state.input);
    for (u32 key_index = 0;
         key_index < ArrayCount(os_input->key_states);
         ++key_index)
    {
        os_input->key_states[key_index] &= ~(InputInteract_Released | InputInteract_Pressed);
    }
    
    MSG message;
    while (PeekMessageA(&message, null, 0, 0, PM_REMOVE) != FALSE)
    {
        switch (message.message)
        {
            case WM_QUIT:
            {
                os_input->misc_flags |= InputFlag_Quit;
            } break;
            
            case WM_KEYDOWN:
            {
                Key_Code key_code = W32_MapWParamToKeyCode(message.wParam);
                if (key_code != KeyCode_Total)
                {
                    os_input->key_states[key_code] |= (InputInteract_Pressed | InputInteract_Held);
                }
            } break;
            
            case WM_KEYUP:
            {
                Key_Code key_code = W32_MapWParamToKeyCode(message.wParam);
                if (key_code != KeyCode_Total)
                {
                    os_input->key_states[key_code] &= ~(InputInteract_Held);
                    os_input->key_states[key_code] |= (InputInteract_Released);
                }
            } break;
            
            default:
            {
                TranslateMessage(&message);
                DispatchMessage(&message);
            } break;
        }
    }
}

function HWND
W32_AcquireWindow(String_Const_U8 window_name, s32 width, s32 height)
{
    HWND result = null;
    
    WNDCLASSA window_class;
    window_class.style = 0;
    window_class.lpfnWndProc = &W32_WindowProc;
    window_class.cbClsExtra = 0;
    window_class.cbWndExtra = 0;
    window_class.hInstance = GetModuleHandleA(null);
    window_class.hIcon = LoadIconA(null, IDI_APPLICATION);
    window_class.hCursor = LoadCursorA(null, IDC_ARROW);
    window_class.hbrBackground = null;
    window_class.lpszMenuName = null;
    window_class.lpszClassName = "bytepath_class";
    
    if (RegisterClassA(&window_class))
    {
        RECT client_area;
        client_area.left = client_area.top = 0;
        client_area.right = width;
        client_area.bottom = height;
        
        if (AdjustWindowRect(&client_area, WS_OVERLAPPEDWINDOW, FALSE) != FALSE)
        {
            result = CreateWindowExA(0, window_class.lpszClassName, window_name.str,
                                     WS_OVERLAPPEDWINDOW, 0, 0,
                                     client_area.right - client_area.left,
                                     client_area.bottom - client_area.top,
                                     null, null, window_class.hInstance,
                                     null);
        }
    }
    
    return(result);
}

function IDXGISwapChain1 *
D3D11_AcquireSwapChain(HWND window_handle, ID3D11Device1 *device1)
{
    IDXGIDevice2 *dxgi_device = null;
    IDXGIAdapter *dxgi_adapter = null;
    IDXGIFactory2 *dxgi_factory = null;
    
    IDXGISwapChain1 *result = null;
    
    HRESULT hresult = ID3D11Device_QueryInterface(device1, &IID_IDXGIDevice2, (void **)(&dxgi_device));
    if (hresult == S_OK)
    {
        hresult = IDXGIDevice2_GetAdapter(dxgi_device, &dxgi_adapter);
        if (hresult == S_OK)
        {
            hresult = IDXGIAdapter_GetParent(dxgi_adapter, &IID_IDXGIFactory2, (void **)(&dxgi_factory));
            if (hresult == S_OK)
            {
                
                DXGI_SWAP_CHAIN_DESC1 swap_chain_desc1;
                swap_chain_desc1.Width = 800;
                swap_chain_desc1.Height = 450;
                swap_chain_desc1.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                swap_chain_desc1.Stereo = FALSE;
                swap_chain_desc1.SampleDesc.Count = 1;
                swap_chain_desc1.SampleDesc.Quality = 0;
                swap_chain_desc1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
                swap_chain_desc1.BufferCount = 2;
                swap_chain_desc1.Scaling = DXGI_SCALING_STRETCH;
                swap_chain_desc1.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
                swap_chain_desc1.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
                swap_chain_desc1.Flags = 0;
                
                hresult = IDXGIFactory2_CreateSwapChainForHwnd(dxgi_factory, (IUnknown *)device1, window_handle, &swap_chain_desc1,
                                                               null, null, &result);
                
                if (hresult != S_OK)
                {
                }
                
                IDXGIFactory2_Release(dxgi_factory);
            }
            
            IDXGIAdapter_Release(dxgi_adapter);
        }
        
        IDXGIDevice2_Release(dxgi_device);
    }
    
    return(result);
}

typedef union v2f
{
    struct
    {
        f32 x, y;
    };
    f32 v[2];
} v2f;

typedef union v4f
{
    struct
    {
        f32 x, y, z, w;
    };
    struct
    {
        f32 r, g, b, a;
    };
    struct
    {
        v2f xy;
        v2f zw;
    };
    f32 v[4];
} v4f;

typedef union m44
{
    struct
    {
        v4f r0, r1, r2, r3;
    };
    f32 m[4][4];
} m44;

typedef struct Quad
{
    v2f origin;
    v2f x_axis;
    v2f y_axis;
    v4f colours[4];
    
    f32 side_roundness;
    f32 side_thickness;
} Quad;

#define V2F(x,y) (v2f){x,y}
#define V4F(x,y,z,w) (v4f){x,y,z,w}
#define RGBA(r,g,b,a) V4F(r,g,b,a)

function m44
Matrix4x4_Orthographic_LH_RM_Z01(f32 left, f32 right, f32 top,
                                 f32 bottom, f32 near, f32 far)
{
    m44 result;
    result.r0 = V4F(2.0f / (right - left), 0.0f, 0.0f, 0.0f);
    result.r1 = V4F(0.0f, 2.0f / (top - bottom), 0.0f, 0.0f);
    result.r2 = V4F(0.0f, 0.0f, 1.0f / (far - near), 0.0f);
    result.r3 = V4F(-(right + left) / (right - left), -(top + bottom) / (top - bottom) , -near / (far - near), 1.0f);
    return(result);
}

function m44
Matrix4x4_Orthographic_LH_CM_Z01(f32 left, f32 right, f32 top,
                                 f32 bottom, f32 near, f32 far)
{
    m44 result;
    result.r0 = V4F(2.0f / (right - left), 0.0f, 0.0f, -(right + left) / (right - left));
    result.r1 = V4F(0.0f, 2.0f / (top - bottom), 0.0f, -(top + bottom) / (top - bottom));
    result.r2 = V4F(0.0f, 0.0f, 1.0f / (far - near), -near / (far - near));
    result.r3 = V4F(0.0f, 0.0f , 0.0f, 1.0f);
    return(result);
}

#define maximum_quads 4096
typedef struct Quad_Render_Batch
{
    Quad quads[maximum_quads];
    u32 quads_drawn;
} Quad_Render_Batch;

inline Quad *
RenderBatch_Acquire(Quad_Render_Batch *render_batch)
{
    Assert(render_batch->quads_drawn < maximum_quads);
    Quad *result = render_batch->quads + render_batch->quads_drawn++;
    return(result);
}

inline Quad *
RenderBatch_Push(Quad_Render_Batch *render_batch, v2f origin, v2f x_axis, v2f y_axis,
                 v4f colour_tl, v4f colour_tr, v4f colour_br, v4f colour_bl, f32 side_roundness,
                 f32 side_thickness)
{
    Quad *quad = RenderBatch_Acquire(render_batch);
    quad->origin = origin;
    quad->x_axis = x_axis;
    quad->y_axis = y_axis;
    quad->colours[0] = colour_tl;
    quad->colours[1] = colour_tr;
    quad->colours[2] = colour_bl;
    quad->colours[3] = colour_br;
    quad->side_roundness = side_roundness;
    quad->side_thickness = side_thickness;
    return(quad);
}

inline Quad *
RenderBatch_PushRectFilled(Quad_Render_Batch *render_batch,
                           v2f origin, v2f dims,
                           v4f colour, f32 roundness)
{
    return RenderBatch_Push(render_batch, origin, V2F(dims.x, 0.0f), V2F(0.0f, dims.y),
                            colour, colour, colour, colour, roundness, 0.0f);
}

inline Quad *
RenderBatch_PushRectOutline(Quad_Render_Batch *render_batch,
                            v2f origin, v2f dims,
                            v4f colour, f32 roundness, f32 thickness)
{
    return RenderBatch_Push(render_batch, origin, V2F(dims.x, 0.0f), V2F(0.0f, dims.y),
                            colour, colour, colour, colour, roundness, thickness);
}

inline Quad *
RenderBatch_PushCircleFilled(Quad_Render_Batch *render_batch,
                             v2f origin, v4f colour, f32 radius)
{
    return RenderBatch_PushRectFilled(render_batch,
                                      V2F(origin.x - radius, origin.y - radius),
                                      V2F(radius * 2.0f, radius * 2.0f),
                                      colour, radius);
}

inline Quad *
RenderBatch_PushCircleOutline(Quad_Render_Batch *render_batch,
                              v2f origin, v4f colour, f32 radius, f32 thickness)
{
    return RenderBatch_PushRectOutline(render_batch,
                                       V2F(origin.x - radius, origin.y - radius),
                                       V2F(radius * 2.0f, radius * 2.0f),
                                       colour, radius, thickness);
}

// TODO(christian): UTF-16 variant
function String_Decode
StringDecode_UTF8(u8 *str, u32 capacity)
{
    String_Decode result = { 0, 0 };
    
    local const u8 lengths_table[] = 
    {
        1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1,
        0, 0, 0, 0, 0, 0, 0, 0,
        2, 2, 2, 2, 3, 3, 4, 0
    };
    
    local const u8 masks_table_based_on_length[] =
    {
        0x00, 0x7F, 0x1F, 0x0F, 0x07,
    };
    
    local const u8 shifts_back_table_based_on_length[] =
    {
        0, 18, 12, 6, 0
    };
    
    result.byte_count = lengths_table[str[0] >> 3];
    result.next_in_str = str + result.byte_count + !result.byte_count;
    
    result.codepoint = (u32)(str[0] & masks_table_based_on_length[result.byte_count]) << 18u;
    result.codepoint |= (u32)(str[1] & 0x3F) << 12u;
    result.codepoint |= (u32)(str[2] & 0x3F) << 6u;
    result.codepoint |= (u32)(str[3] & 0x3F) << 0u;
    result.codepoint >>= shifts_back_table_based_on_length[result.byte_count];
    
    return(result);
}

// TODO(christian): UTF-16 variant
function u32
StringEncode_UTF8(u8 *dest, u32 codepoint)
{
    u32 byte_count_result = 0;
    
    if (codepoint < 0xFFu)
    {
        dest[0] = (u8)codepoint;
        byte_count_result = 1;
    }
    else if (codepoint < (0xFFFu))
    {
        dest[0] = (u8)((codepoint >> 6) | 0xC0);
        dest[1] = (u8)((codepoint & 0x3F) | 0x80);
        byte_count_result = 2;
    }
    else if (codepoint < 0xFFFFu)
    {
        dest[0] = (u8)((codepoint >> 12) | 0xE0);
        dest[1] = (u8)(((codepoint >> 6) & 0x3F) | 0x80);
        dest[2] = (u8)(((codepoint >> 0) & 0x3F) | 0x80);
        byte_count_result = 3;
    } else if (codepoint < 0x200000u)
    {
        dest[0] = (u8)((codepoint >> 18) | 0xF0);
        dest[1] = (u8)(((codepoint >> 12) & 0x3F) | 0x80);
        dest[2] = (u8)(((codepoint >> 6) & 0x3F) | 0x80);
        dest[3] = (u8)(((codepoint >> 0) & 0x3F) | 0x80);
        byte_count_result = 4;
    }
    
    return(byte_count_result);
}

function f32
EaseOutQuart(f32 t)
{
    f32 result = 1.0f - powf(1.0f - t, 4.0f);
    return(result);
}

function f32
EaseInQuart(f32 t)
{
    f32 result = t * t * t * t;
    return(result);
}

s32 main(void)
{
    HWND window_handle = W32_AcquireWindow(Str8Lit("Hi"), 1280, 720);
    if (IsWindow(window_handle))
    {
        ShowWindow(window_handle, SW_SHOW);
        
        TIMECAPS time_caps;
        if (timeGetDevCaps(&time_caps, sizeof(time_caps)) == MMSYSERR_NOERROR)
        {
            timeBeginPeriod(time_caps.wPeriodMin);
            
            PROCESS_POWER_THROTTLING_STATE process_throttling_state;
            process_throttling_state.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
            process_throttling_state.ControlMask = PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION;
            process_throttling_state.StateMask = 0;
            
            SetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling,
                                  (void *)(&process_throttling_state), sizeof(PROCESS_POWER_THROTTLING_STATE));
            
            LARGE_INTEGER ticks_per_second_li;
            QueryPerformanceFrequency(&ticks_per_second_li);
            w32_ticks_per_second = (u64)ticks_per_second_li.QuadPart;
        }
        
        const s32 refresh_rate = W32_GetRefreshMonitorRefreshRate(window_handle);
        const f32 seconds_per_frame = 1.0f / (f32)refresh_rate;
        const f32 delta_time = seconds_per_frame;
        
        ID3D11Device *base_device = null;
        ID3D11Device1 *main_device = null;
        ID3D11DeviceContext *base_device_context = null;
        IDXGISwapChain1 *dxgi_swap_chain = null;
        ID3D11Texture2D *back_buffer = null;
        ID3D11RenderTargetView *render_target_view = null;
        ID3D11RasterizerState1 *fill_no_cull_rasterizer_state = null;
        ID3D11BlendState *render_blend_state = null;
        
        ID3D11VertexShader *main_vertex_shader = null;
        ID3D11PixelShader *main_pixel_shader = null;
        ID3D11Buffer *quad_sb = null;
        ID3D11ShaderResourceView *quad_srv = null;
        ID3D11Buffer *quad_renderer_constants = null;
        
        D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_0;
        HRESULT hresult = D3D11CreateDevice(null, D3D_DRIVER_TYPE_HARDWARE, null, D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_DEBUG,
                                            &feature_level, 1, D3D11_SDK_VERSION, &base_device, null, &base_device_context);
        
        // NOTE(christian): logging
        if (hresult == S_OK)
        {
            hresult = ID3D11Device_QueryInterface(base_device, &IID_ID3D11Device1, (void **)(&main_device));
            
            dxgi_swap_chain = D3D11_AcquireSwapChain(window_handle, main_device);
            
            hresult = IDXGISwapChain1_GetBuffer(dxgi_swap_chain, 0, &IID_ID3D11Texture2D,
                                                (void **)(&back_buffer));
            
            D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = {0};
            rtv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
            rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
            
            ID3D11Device1_CreateRenderTargetView(main_device, (ID3D11Resource *)back_buffer, &rtv_desc,
                                                 &render_target_view);
            
            ID3DBlob *bytecode_blob = null;
            ID3DBlob *error_blob = null;
            
            D3DCompileFromFile(L"..\\data\\shaders\\main_shader.hlsl", null, null, "VSMain",
                               "vs_5_0", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0,
                               &bytecode_blob, &error_blob);
            
            if (!error_blob)
            {
                ID3D11Device1_CreateVertexShader(main_device, ID3D10Blob_GetBufferPointer(bytecode_blob),
                                                 ID3D10Blob_GetBufferSize(bytecode_blob), null,
                                                 &main_vertex_shader);
                
                ID3D10Blob_Release(bytecode_blob);
                bytecode_blob = null;
            }
            else
            {
                printf(ID3D10Blob_GetBufferPointer(error_blob));
                ID3D10Blob_Release(error_blob);
                error_blob = null;
                
                Assert(0);
            }
            
            D3DCompileFromFile(L"..\\data\\shaders\\main_shader.hlsl", null, null, "PSMain",
                               "ps_5_0", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0,
                               &bytecode_blob, &error_blob);
            
            if (!error_blob)
            {
                ID3D11Device1_CreatePixelShader(main_device, ID3D10Blob_GetBufferPointer(bytecode_blob),
                                                ID3D10Blob_GetBufferSize(bytecode_blob), null,
                                                &main_pixel_shader);
                
                ID3D10Blob_Release(bytecode_blob);
                bytecode_blob = null;
            }
            else
            {
                printf(ID3D10Blob_GetBufferPointer(error_blob));
                ID3D10Blob_Release(error_blob);
                error_blob = null;
                
                Assert(0);
            }
            
            D3D11_RASTERIZER_DESC1 raster_desc1;
            raster_desc1.FillMode = D3D11_FILL_SOLID;
            raster_desc1.CullMode = D3D11_CULL_NONE;
            raster_desc1.FrontCounterClockwise;
            raster_desc1.DepthBias = 0;
            raster_desc1.DepthBiasClamp = 0.0f;
            raster_desc1.SlopeScaledDepthBias;
            raster_desc1.DepthClipEnable = TRUE;
            raster_desc1.ScissorEnable = FALSE;
            raster_desc1.MultisampleEnable = FALSE;
            raster_desc1.AntialiasedLineEnable = FALSE;
            raster_desc1.ForcedSampleCount = FALSE;
            ID3D11Device1_CreateRasterizerState1(main_device, &raster_desc1, &fill_no_cull_rasterizer_state);
            
            D3D11_BLEND_DESC blend_desc = {0};
            blend_desc.AlphaToCoverageEnable = FALSE;
            blend_desc.IndependentBlendEnable = FALSE;
            blend_desc.RenderTarget[0].BlendEnable = TRUE;
            blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
            blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
            blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
            blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
            blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
            blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
            blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
            ID3D11Device1_CreateBlendState(main_device, &blend_desc, &render_blend_state);
        }
        
        D3D11_BUFFER_DESC quad_sb_desc;
        quad_sb_desc.ByteWidth = maximum_quads * sizeof(Quad);
        quad_sb_desc.Usage = D3D11_USAGE_DYNAMIC;
        quad_sb_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        quad_sb_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        quad_sb_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        quad_sb_desc.StructureByteStride = sizeof(Quad);
        ID3D11Device1_CreateBuffer(main_device, &quad_sb_desc, null, &quad_sb);
        
        D3D11_SHADER_RESOURCE_VIEW_DESC quad_srv_desc;
        quad_srv_desc.Format = DXGI_FORMAT_UNKNOWN;
        quad_srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        quad_srv_desc.Buffer.NumElements = maximum_quads;
        ID3D11Device1_CreateShaderResourceView(main_device, (ID3D11Resource *)quad_sb, &quad_srv_desc, &quad_srv);
        
        D3D11_BUFFER_DESC constant_buffer_desc;
        constant_buffer_desc.ByteWidth = (sizeof(m44) + 15) & ~(15);
        constant_buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
        constant_buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        constant_buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        constant_buffer_desc.MiscFlags = 0;
        constant_buffer_desc.StructureByteStride = 0;
        ID3D11Device1_CreateBuffer(main_device, &constant_buffer_desc, null, &quad_renderer_constants);
        
        D3D11_TEXTURE2D_DESC back_buffer_desc;
        ID3D11Texture2D_GetDesc(back_buffer, &back_buffer_desc);
        
        D3D11_VIEWPORT viewport;
        viewport.TopLeftX = 0.0f;
        viewport.TopLeftY = 0.0f;
        viewport.Width = (f32)back_buffer_desc.Width;
        viewport.Height = (f32)back_buffer_desc.Height;
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        
        Quad_Render_Batch quad_render_batch;
        
        SeedRandom_U32((u32)time(null));
        u64 begin_ticks = W32_GetTicks();
        
        while (!OS_InputFlagGet(InputFlag_Quit))
        {
            quad_render_batch.quads_drawn = 0;
            W32_FillEvents();
            if (OS_KeyReleased(KeyCode_Escape))
            {
                OS_InputFlagSet(InputFlag_Quit, True);
            }
            
            RenderBatch_PushCircleOutline(&quad_render_batch, V2F(viewport.Width * 0.5f, viewport.Height * 0.5f),
                                          RGBA(1.0f, 0.0f, 0.0f, 1.0f), 40.0f, 1.0f);
            
            for (f32 gradient_index = 0; gradient_index < 255.0f; gradient_index += 5.0f)
            {
                RenderBatch_PushRectFilled(&quad_render_batch, V2F(gradient_index, 10.0f), V2F(6.0f, 50.0f),
                                           RGBA(gradient_index / 255.0f, 0.0f, 0.0f, 1.0f), 0.0f);
            }
            
            if (quad_render_batch.quads_drawn)
            {
                D3D11_MAPPED_SUBRESOURCE mapped_subresource;
                
                switch (ID3D11DeviceContext_Map(base_device_context, (ID3D11Resource *)quad_sb, 
                                                0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_subresource))
                {
                    case S_OK:
                    {
                        MemoryCopy(mapped_subresource.pData, quad_render_batch.quads, sizeof(quad_render_batch.quads));
                        ID3D11DeviceContext_Unmap(base_device_context, (ID3D11Resource *)quad_sb, 0);
                    } break;
                }
            }
            
            D3D11_MAPPED_SUBRESOURCE mapped_subresource;
            switch (ID3D11DeviceContext_Map(base_device_context, (ID3D11Resource *)quad_renderer_constants, 
                                            0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_subresource))
            {
                case S_OK:
                {
                    m44 test = Matrix4x4_Orthographic_LH_CM_Z01(0.0f, viewport.Width,
                                                                0.0f, viewport.Height,
                                                                0.0f, 1.0f);
                    MemoryCopy(mapped_subresource.pData, &test, sizeof(m44));
                    ID3D11DeviceContext_Unmap(base_device_context, (ID3D11Resource *)quad_renderer_constants, 0);
                } break;
            }
            
            f32 clear_colour[] = { 0.0f, 0.0f, 0.0f, 1.0f };
            
            ID3D11DeviceContext_ClearRenderTargetView(base_device_context, render_target_view, clear_colour);
            
            ID3D11DeviceContext_IASetPrimitiveTopology(base_device_context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
            
            ID3D11DeviceContext_VSSetConstantBuffers(base_device_context, 0, 1, &quad_renderer_constants);
            ID3D11DeviceContext_VSSetShaderResources(base_device_context, 0, 1, &quad_srv);
            ID3D11DeviceContext_VSSetShader(base_device_context, main_vertex_shader, null, 0);
            
            ID3D11DeviceContext_RSSetState(base_device_context, (ID3D11RasterizerState *)fill_no_cull_rasterizer_state);
            ID3D11DeviceContext_RSSetViewports(base_device_context, 1, &viewport);
            
            ID3D11DeviceContext_PSSetShader(base_device_context, main_pixel_shader, null, 0);
            
            ID3D11DeviceContext_OMSetRenderTargets(base_device_context, 1, &render_target_view, null);
            ID3D11DeviceContext_OMSetBlendState(base_device_context, render_blend_state, null, 0xFFFFFFFF);
            
            if (quad_render_batch.quads_drawn)
            {
                ID3D11DeviceContext_DrawInstanced(base_device_context, 4, quad_render_batch.quads_drawn, 0, 0);
            }
            
            IDXGISwapChain1_Present(dxgi_swap_chain, 1, 0); 
            
            u64 end_ticks = W32_GetTicks();
            f32 seconds_elapsed_for_frame = W32_SecondsBetweenTicksF32(begin_ticks, end_ticks);
            while (seconds_elapsed_for_frame < seconds_per_frame)
            {
                DWORD sleep_ms = (DWORD)((f32)(seconds_per_frame - seconds_elapsed_for_frame) * 1000.0f);
                if (sleep_ms)
                {
                    OS_Sleep(sleep_ms);
                }
                
                end_ticks = W32_GetTicks();
                seconds_elapsed_for_frame = W32_SecondsBetweenTicksF32(begin_ticks, end_ticks);
            }
            
            begin_ticks = end_ticks;
        }
        
        timeEndPeriod(time_caps.wPeriodMin);
    }
    
    return(0);
}