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
    KeyCode_LeftArrow,
    KeyCode_UpArrow,
    KeyCode_RightArrow,
    KeyCode_DownArrow,
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
        
        case VK_LEFT:
        {
            result = KeyCode_LeftArrow;
        } break;
        
        case VK_UP:
        {
            result = KeyCode_UpArrow;
        } break;
        
        case VK_RIGHT:
        {
            result = KeyCode_RightArrow;
        } break;
        
        case VK_DOWN:
        {
            result = KeyCode_DownArrow;
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
W32_GetMonitorRefreshRate(HWND window_handle)
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
                swap_chain_desc1.Width = 480;
                swap_chain_desc1.Height = 270;
                swap_chain_desc1.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                swap_chain_desc1.Stereo = FALSE;
                swap_chain_desc1.SampleDesc.Count = 1;
                swap_chain_desc1.SampleDesc.Quality = 0;
                swap_chain_desc1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
                swap_chain_desc1.BufferCount = 2;
                swap_chain_desc1.Scaling = DXGI_SCALING_STRETCH;
                swap_chain_desc1.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
                swap_chain_desc1.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
                //swap_chain_desc1.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
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

#define maximum_quads 4096
typedef struct Quad_Render_Batch
{
    Quad quads[maximum_quads];
    u32 quads_drawn;
} Quad_Render_Batch;

inline Quad *
QuadRenderBatch_Acquire(Quad_Render_Batch *render_batch)
{
    Assert(render_batch->quads_drawn < maximum_quads);
    Quad *result = render_batch->quads + render_batch->quads_drawn++;
    return(result);
}

inline Quad *
QuadRenderBatch_Push(Quad_Render_Batch *render_batch, v2f origin, v2f x_axis, v2f y_axis,
                     v4f colour_tl, v4f colour_tr, v4f colour_br, v4f colour_bl, f32 side_roundness,
                     f32 side_thickness)
{
    Quad *quad = QuadRenderBatch_Acquire(render_batch);
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
QuadRenderBatch_PushRectFilled(Quad_Render_Batch *render_batch,
                               v2f origin, v2f dims,
                               v4f colour, f32 roundness)
{
    return QuadRenderBatch_Push(render_batch, origin, V2F(dims.x, 0.0f), V2F(0.0f, dims.y),
                                colour, colour, colour, colour, roundness, 0.0f);
}

inline Quad *
QuadRenderBatch_PushRectOutline(Quad_Render_Batch *render_batch,
                                v2f origin, v2f dims,
                                v4f colour, f32 roundness, f32 thickness)
{
    return QuadRenderBatch_Push(render_batch, origin, V2F(dims.x, 0.0f), V2F(0.0f, dims.y),
                                colour, colour, colour, colour, roundness, thickness);
}

inline Quad *
QuadRenderBatch_PushCircleFilled(Quad_Render_Batch *render_batch,
                                 v2f origin, v4f colour, f32 radius)
{
    return QuadRenderBatch_PushRectFilled(render_batch,
                                          V2F(origin.x - radius, origin.y - radius),
                                          V2F(radius * 2.0f, radius * 2.0f),
                                          colour, radius);
}

inline Quad *
QuadRenderBatch_PushCircleOutline(Quad_Render_Batch *render_batch,
                                  v2f origin, v4f colour, f32 radius, f32 thickness)
{
    return QuadRenderBatch_PushRectOutline(render_batch,
                                           V2F(origin.x - radius, origin.y - radius),
                                           V2F(radius * 2.0f, radius * 2.0f),
                                           colour, radius, thickness);
}

// TODO(christian): immediate mode rending api.
/*
we cannot limit ourselves with quads. we want to draw convex / nonconvex polygons,
lines, triangles, and so on. We might leave the quad rendering batch for the UI system.
knowing this, we do instanced rendering for quad rendering, and issue draw calls for
immediate rendering

// we also cannot just say "draw line" or "draw triangle" becasue how would d3d11 interpret
that? we need some sort of "draw call" struct that records what of primitive we have drawn.
and thus we input the number of vertices used to d3d. we also might want an index to the vertices
array in render_batch
*/

typedef enum Render_Primitive_Kind
{
    RenderPrimitiveKind_None,
    RenderPrimitiveKind_Point, // 1 vertices. D3D11_PRIMITIVE_TOPOLOGY_POINTLIST.
    RenderPrimitiveKind_Line, // 2 vertices. use D3D11_PRIMITIVE_TOPOLOGY_LINELIST / D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP.
    RenderPrimitiveKind_Triangle, // 3 vertices. D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP / D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
    RenderPrimitiveKind_Quad, // 4 vertices. Use D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP
} Render_Primitive_Kind;

typedef struct Render_Per_Vertex_Data
{
    v2f vertex;
    v4f colour;
} Render_Per_Vertex_Data;

typedef struct Render_Draw_Call
{
    Render_Primitive_Kind primitive_kind;
    u32 vertex_array_base_index;
    u32 vertex_array_end_index;
    b32 filled;
} Render_Draw_Call;

// TODO(christian): no stack allocate
#define max_draw_calls 2048
#define max_vertices 8192
typedef struct Render_Batch
{
    Render_Draw_Call draw_calls[max_draw_calls];
    u32 draw_call_count;
    
    Render_Per_Vertex_Data vertices[max_vertices];
    u32 vertex_count;
    
    b32 has_begun;
    b32 filled;
    Render_Primitive_Kind current_primitive;
    u32 current_vertex_array_start;
    v4f current_colour;
} Render_Batch;

inline void
RenderBatch_BeginPrimitive(Render_Batch *render_batch, Render_Primitive_Kind kind, b32 filled)
{
    AssertFalse(render_batch->has_begun);
    AssertTrue(render_batch->draw_call_count < max_draw_calls);
    render_batch->has_begun = True;
    render_batch->current_primitive = kind;
    render_batch->current_vertex_array_start = render_batch->vertex_count;
    if (kind != RenderPrimitiveKind_Line)
    {
        render_batch->filled = filled;
    }
    else
    {
        render_batch->filled = True;
    }
}

inline void
RenderBatch_End(Render_Batch *render_batch)
{
    AssertTrue(render_batch->has_begun);
    AssertTrue(render_batch->draw_call_count < max_draw_calls);
    AssertTrue(render_batch->current_primitive != RenderPrimitiveKind_None);
    AssertTrue(render_batch->current_vertex_array_start != bad_index_u32);
    
    // NOTE(christian): did we pushed something?
    if (render_batch->current_vertex_array_start < render_batch->vertex_count)
    {
        // NOTE(christian): then new draw call!
        render_batch->has_begun = False;
        
        Render_Draw_Call *draw_call = render_batch->draw_calls + render_batch->draw_call_count++;
        draw_call->primitive_kind = render_batch->current_primitive;
        draw_call->vertex_array_base_index = render_batch->current_vertex_array_start;
        draw_call->vertex_array_end_index = render_batch->vertex_count;
        draw_call->filled = render_batch->filled;
    }
    
    render_batch->current_primitive = RenderPrimitiveKind_None;
    render_batch->current_vertex_array_start = bad_index_u32;
    render_batch->filled = False;
}

inline v4f
RenderBatch_Colour(Render_Batch *render_batch, v4f colour)
{
    v4f old = render_batch->current_colour;
    render_batch->current_colour = colour;
    return(old);
}

inline void
RenderBatch_Vertex(Render_Batch *render_batch, v2f v)
{
    Assert(render_batch->vertex_count < max_vertices);
    AssertTrue(render_batch->has_begun);
    Render_Per_Vertex_Data *vertex = render_batch->vertices + render_batch->vertex_count++;
    vertex->vertex = v;
    vertex->colour = render_batch->current_colour;
}

function void
RenderBatch_PushLine(Render_Batch *render_batch, v2f start, v2f end, v4f colour)
{
    Render_Draw_Call draw_call;
    draw_call.primitive_kind = RenderPrimitiveKind_Line;
    draw_call.vertex_array_base_index = render_batch->vertex_count;
    
    render_batch->draw_calls[render_batch->draw_call_count++] = draw_call;
    
    
    Render_Per_Vertex_Data *vertex = render_batch->vertices + render_batch->vertex_count;
    render_batch->vertex_count += 2;
    
    vertex->vertex = start;
    vertex->colour = colour;
    
    ++vertex;
    
    vertex->vertex = end;
    vertex->colour = colour;
}

// TODO(christian): this is bad in performance (alot of trigonom).... do midpoint or cache the sin cos and scale
function void
RenderBatch_PushCircleOutline(Render_Batch *render_batch, v2f origin, v4f colour, f32 radius)
{
    RenderBatch_BeginPrimitive(render_batch, RenderPrimitiveKind_Line, True);
    v4f old = RenderBatch_Colour(render_batch, colour);
    
    f32 theta_step = 0.01745329251f * 6.0f; // 6 degrees
    
    for (f32 theta = 0; theta <= two_pi_F32; theta += theta_step)
    {
        RenderBatch_Vertex(render_batch, V2F(radius * cosf(theta) + origin.x, radius * sinf(theta) + origin.y));
    }
    
    RenderBatch_Colour(render_batch, old);
    RenderBatch_End(render_batch);
}

typedef struct Memory_Arena
{
    u8 *memory;
    u64 capacity;
    u64 stack_ptr;
    u64 commit_ptr;
} Memory_Arena;

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
        
        const s32 refresh_rate = W32_GetMonitorRefreshRate(window_handle);
        const f32 seconds_per_frame = 1.0f / (f32)refresh_rate;
        const f32 delta_time = seconds_per_frame;
        
        ID3D11Device *base_device = null;
        ID3D11Device1 *main_device = null;
        ID3D11DeviceContext *base_device_context = null;
        IDXGISwapChain1 *dxgi_swap_chain = null;
        ID3D11Texture2D *back_buffer = null;
        ID3D11RenderTargetView *render_target_view = null;
        ID3D11RasterizerState1 *fill_no_cull_rasterizer_state = null;
        ID3D11RasterizerState1 *wire_no_cull_rasterizer_state = null;
        ID3D11BlendState *render_blend_state = null;
        
        ID3D11VertexShader *main_vertex_shader = null;
        ID3D11PixelShader *main_pixel_shader = null;
        ID3D11Buffer *quad_sb = null;
        ID3D11ShaderResourceView *quad_srv = null;
        ID3D11Buffer *quad_renderer_constants = null;
        
        ID3D11VertexShader *immediate_vertex_shader = null;
        ID3D11PixelShader *immediate_pixel_shader = null;
        ID3D11Buffer *render_batch_vertex_buffer = null;
        ID3D11InputLayout *render_batch_input_layout = null;
        
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
            
            //~
            D3DCompileFromFile(L"..\\data\\shaders\\immediate_render.hlsl", null, null, "VSMain",
                               "vs_5_0", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0,
                               &bytecode_blob, &error_blob);
            
            if (!error_blob)
            {
                ID3D11Device1_CreateVertexShader(main_device, ID3D10Blob_GetBufferPointer(bytecode_blob),
                                                 ID3D10Blob_GetBufferSize(bytecode_blob), null,
                                                 &immediate_vertex_shader);
                
                
                D3D11_INPUT_ELEMENT_DESC input_laypout_desc[] = 
                {
                    (D3D11_INPUT_ELEMENT_DESC){
                        "Vertex", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0
                    },
                    (D3D11_INPUT_ELEMENT_DESC){
                        "Colour", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0
                    }
                };
                
                ID3D11Device1_CreateInputLayout(main_device, input_laypout_desc, 2, ID3D10Blob_GetBufferPointer(bytecode_blob),
                                                ID3D10Blob_GetBufferSize(bytecode_blob), &render_batch_input_layout);
                
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
            
            D3DCompileFromFile(L"..\\data\\shaders\\immediate_render.hlsl", null, null, "PSMain",
                               "ps_5_0", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0,
                               &bytecode_blob, &error_blob);
            
            if (!error_blob)
            {
                ID3D11Device1_CreatePixelShader(main_device, ID3D10Blob_GetBufferPointer(bytecode_blob),
                                                ID3D10Blob_GetBufferSize(bytecode_blob), null,
                                                &immediate_pixel_shader);
                
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
            
            //~
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
            
            raster_desc1.FillMode = D3D11_FILL_WIREFRAME;
            ID3D11Device1_CreateRasterizerState1(main_device, &raster_desc1, &wire_no_cull_rasterizer_state);
            
            D3D11_BLEND_DESC blend_desc = {0};
            blend_desc.AlphaToCoverageEnable = FALSE;
            blend_desc.IndependentBlendEnable = FALSE;
            blend_desc.RenderTarget[0].BlendEnable = TRUE;
            blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
            blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
            blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
            blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
            blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
            blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
            blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
            ID3D11Device1_CreateBlendState(main_device, &blend_desc, &render_blend_state);
        }
        
        //~ NOTE(christian): general rendering
        
        D3D11_BUFFER_DESC vertex_buffer_desc;
        vertex_buffer_desc.ByteWidth = sizeof(Render_Per_Vertex_Data) * 512; // max 1024 vertices
        vertex_buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
        vertex_buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        vertex_buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        vertex_buffer_desc.MiscFlags = 0;
        vertex_buffer_desc.StructureByteStride = 0;
        
        ID3D11Device1_CreateBuffer(main_device, &vertex_buffer_desc, null, 
                                   &render_batch_vertex_buffer);
        
        //~ NOTE(christian): quad rendering
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
        
        Render_Batch render_batch = {0};
        
        // NOTE(christian): our ship has 0 accel. constant velocity.
        f32 circle_theta_angle_radians = 0.0f;
        f32 circle_speed = 40.0f;
        v2f circle_p = V2F(viewport.Width * 0.5f, viewport.Height * 0.5f);
        while (!OS_InputFlagGet(InputFlag_Quit))
        {
            quad_render_batch.quads_drawn = 0;
            render_batch.draw_call_count = 0;
            render_batch.vertex_count = 0;
            W32_FillEvents();
            if (OS_KeyReleased(KeyCode_Escape))
            {
                OS_InputFlagSet(InputFlag_Quit, True);
            }
            
            v2f dP = V2F(0, 0);
            
            if (OS_KeyHeld(KeyCode_UpArrow))
            {
                dP.x = cosf(circle_theta_angle_radians);
                dP.y = sinf(circle_theta_angle_radians);
            }
            
            if (OS_KeyHeld(KeyCode_DownArrow))
            {
                dP.x = -cosf(circle_theta_angle_radians);
                dP.y = -sinf(circle_theta_angle_radians);
            }
            
            if (OS_KeyHeld(KeyCode_RightArrow))
            {
                circle_theta_angle_radians += delta_time;
            }
            
            if (OS_KeyHeld(KeyCode_LeftArrow))
            {
                circle_theta_angle_radians -= delta_time;
            }
            
            if (circle_theta_angle_radians >= two_pi_F32)
            {
                circle_theta_angle_radians = 0.0f;
            }
            else if (circle_theta_angle_radians <= 0.0f)
            {
                circle_theta_angle_radians = two_pi_F32;
            }
            
            if (dP.x && dP.y)
            {
                dP = V2F_Scale(dP, 0.70710678118f);
            }
            
            circle_p = V2F_Add(V2F_Scale(dP, circle_speed * delta_time), circle_p);
            
            //QuadRenderBatch_PushCircleOutline(&quad_render_batch, circle_p, RGBA(1.0f, 0.0f, 0.0f, 1.0f), 10.0f, 1.0f);
            
#if 0
            for (f32 gradient_index = 0; gradient_index < 255.0f; gradient_index += 5.0f)
            {
                QuadRenderBatch_PushRectFilled(&quad_render_batch, V2F(gradient_index, 10.0f), V2F(6.0f, 50.0f),
                                               RGBA(gradient_index / 255.0f, 0.0f, 0.0f, 1.0f), 0.0f);
            }
#endif
            
            RenderBatch_BeginPrimitive(&render_batch, RenderPrimitiveKind_Triangle, False); {
                RenderBatch_Colour(&render_batch, V4F(1.0f, 0.0f, 0.0f, 1.0f));
                RenderBatch_Vertex(&render_batch, V2F(viewport.Width * 0.50f, viewport.Height * 0.25f));
                RenderBatch_Vertex(&render_batch, V2F(viewport.Width * 0.75f, viewport.Height * 0.75f));
                RenderBatch_Vertex(&render_batch, V2F(viewport.Width * 0.25f, viewport.Height * 0.75f));
                
                RenderBatch_Colour(&render_batch, V4F(1.0f, 1.0f, 0.0f, 1.0f));
                RenderBatch_Vertex(&render_batch, V2F(viewport.Width * 0.50f, viewport.Height * 0.35f));
                RenderBatch_Vertex(&render_batch, V2F(viewport.Width * 0.65f, viewport.Height * 0.65f));
                RenderBatch_Vertex(&render_batch, V2F(viewport.Width * 0.35f, viewport.Height * 0.65f));
            } RenderBatch_End(&render_batch);
            
            RenderBatch_PushCircleOutline(&render_batch, circle_p, RGBA(1.0f, 1.0f, 1.0f, 1.0f), 16.0f);
            
            RenderBatch_BeginPrimitive(&render_batch, RenderPrimitiveKind_Line, True); {
                RenderBatch_Colour(&render_batch, V4F(1.0f, 1.0f, 1.0f, 1.0f));
                RenderBatch_Vertex(&render_batch, circle_p);
                RenderBatch_Vertex(&render_batch, V2F(dP.x * circle_speed + circle_p.x, dP.y * circle_speed + circle_p.y));
            } RenderBatch_End(&render_batch);
            
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
            
            f32 clear_colour[] = { powf(0.0f, 2.2f), powf(0.0f, 2.2f), powf(0.0f, 2.2f), 1.0f };
            
            ID3D11DeviceContext_ClearRenderTargetView(base_device_context, render_target_view, clear_colour);
            
            //~ NOTE(christian): commons
            ID3D11DeviceContext_VSSetConstantBuffers(base_device_context, 0, 1, &quad_renderer_constants);
            
            ID3D11DeviceContext_RSSetViewports(base_device_context, 1, &viewport);
            
            ID3D11DeviceContext_OMSetRenderTargets(base_device_context, 1, &render_target_view, null);
            ID3D11DeviceContext_OMSetBlendState(base_device_context, render_blend_state, null, 0xFFFFFFFF);
            
            //~ NOTE(christian): general rendering
            u32 stride = sizeof(Render_Per_Vertex_Data);
            u32 offset = 0;
            ID3D11DeviceContext_IASetInputLayout(base_device_context, render_batch_input_layout);
            ID3D11DeviceContext_IASetVertexBuffers(base_device_context, 0, 1, &render_batch_vertex_buffer, &stride, &offset);
            ID3D11DeviceContext_VSSetShader(base_device_context, immediate_vertex_shader, null, 0);
            
            ID3D11DeviceContext_PSSetShader(base_device_context, immediate_pixel_shader, null, 0);
            
            for (u32 draw_call_index = 0;
                 draw_call_index < render_batch.draw_call_count;
                 ++draw_call_index)
            {
                Render_Draw_Call *draw_call = render_batch.draw_calls + draw_call_index;
                //AssertTrue(draw_call->vertex_array_base_index < draw_call->vertex_array_end_index);
                
                ID3D11RasterizerState *rasterizer = (ID3D11RasterizerState *)(draw_call->filled ? 
                                                                              fill_no_cull_rasterizer_state :
                                                                              wire_no_cull_rasterizer_state);
                u32 vertices = draw_call->vertex_array_end_index - draw_call->vertex_array_base_index;
                if (!(draw_call->vertex_array_base_index < draw_call->vertex_array_end_index))
                {
                    vertices = 2;
                }
                
                switch (draw_call->primitive_kind)
                {
                    // TODO(christian): instead of drawing lines/triangles once, see how much lines/traignles to
                    // draw currently and send to gpu.
                    case RenderPrimitiveKind_Line:
                    {
                        ID3D11DeviceContext_IASetPrimitiveTopology(base_device_context, D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
                    } break;
                    
                    case RenderPrimitiveKind_Triangle:
                    {
                        ID3D11DeviceContext_IASetPrimitiveTopology(base_device_context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                    } break;
                    
                    default:
                    {
                        InvalidCodePath();
                    } break;
                }
                
                switch (ID3D11DeviceContext_Map(base_device_context, (ID3D11Resource *)render_batch_vertex_buffer, 
                                                0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_subresource))
                {
                    case S_OK:
                    {
                        MemoryCopy(mapped_subresource.pData, render_batch.vertices + draw_call->vertex_array_base_index,
                                   sizeof(Render_Per_Vertex_Data) * vertices);
                        ID3D11DeviceContext_Unmap(base_device_context, (ID3D11Resource *)render_batch_vertex_buffer, 0);
                    } break;
                }
                
                ID3D11DeviceContext_RSSetState(base_device_context, rasterizer);
                ID3D11DeviceContext_Draw(base_device_context, vertices, 0);
            }
            
            //~ NOTE(christian): quad rendering
            ID3D11DeviceContext_IASetPrimitiveTopology(base_device_context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
            ID3D11DeviceContext_IASetInputLayout(base_device_context, null);
            ID3D11DeviceContext_IASetVertexBuffers(base_device_context, 0, 0, null, null, null);
            
            ID3D11DeviceContext_VSSetShaderResources(base_device_context, 0, 1, &quad_srv);
            ID3D11DeviceContext_VSSetShader(base_device_context, main_vertex_shader, null, 0);
            
            ID3D11DeviceContext_PSSetShader(base_device_context, main_pixel_shader, null, 0);
            
            ID3D11DeviceContext_RSSetState(base_device_context, (ID3D11RasterizerState *)fill_no_cull_rasterizer_state);
            
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
            
            render_batch.draw_call_count = 0;
        }
        
        timeEndPeriod(time_caps.wPeriodMin);
    }
    
    return(0);
}