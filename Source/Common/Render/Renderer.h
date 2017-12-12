#pragma once

#define RENDERER_D3D12
#define FULLSCREEN 0
#define FRAME_BUFFERS 3
#define PARAMETER_BUFFER_SIZE 65536

#ifdef __cplusplus
extern "C"
{
#endif

#include "../Core/DataTypes.h"

#define DEFAULT_RENDER_TARGET NULL

struct RenderPass;

enum
{
    FL_PASS_CLEAR_COLOR = 1,
    FL_PASS_CLEAR_DEPTH = 2
};

#ifdef RENDERER_D3D12

#include <wtypes.h>

struct D3D12_DeviceContext;
struct D3D12_FrameContext;
struct D3D12_RenderTarget;

struct D3D12_DeviceContext* D3D12_InitDeviceContext(struct D3D12_DeviceContext** outDC, void* window);
void D3D12_GetParameterBuffer(struct D3D12_FrameContext* fc, void** outPtr, uint64_t* outMax);
struct D3D12_FrameContext* D3D12_PrepareFrame(struct D3D12_DeviceContext* dc, struct D3D12_FrameContext** outFC);
void D3D12_BeginRenderPass(struct D3D12_FrameContext* fc, struct RenderPass* pass);
void D3D12_CommitFrame(struct D3D12_FrameContext* fc);

typedef struct D3D12_DeviceContext* RendererDevice;
typedef struct D3D12_FrameContext* RendererFrame;
typedef struct D3D12_RenderTarget* RenderTarget;

#define RendererInitDevice D3D12_InitDeviceContext
#define GetParameterBuffer D3D12_GetParameterBuffer
#define RendererInitFrame D3D12_PrepareFrame
#define BeginRenderPass D3D12_BeginRenderPass
#define EndRenderPass(f, p)
#define RendererCommitFrame D3D12_CommitFrame

#endif

typedef struct RenderPass
{
    struct RenderPass* Next;
    RenderTarget OutputTarget;
    Vec4f ClearColor;
    LPCTSTR Name;
    uint64_t Flags;
} RenderPass;

#ifdef __cplusplus
}
#endif
