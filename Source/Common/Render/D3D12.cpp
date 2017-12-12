#include "Renderer.h"

#ifdef RENDERER_D3D12

#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>

#ifdef _DEBUG
#include <dxgidebug.h>
#include <d3d12sdklayers.h>
#endif

#include <assert.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

#define FULLSCREEN 0
#define FRAME_BUFFERS 3
#define PARAMETER_BUFFER_SIZE 65536
#define APP_NAME L"FirstTriangle"

#define RELEASE(p) if((p)){(p)->Release();(p) = nullptr;}

/*

void Render(struct D3D12_DeviceContext* device)
{
D3D12_FrameContext* frame = D3D12_BeginFrame(device);
assert(frame);

void* paramBuff = nullptr;
uint64_t paramBuffSize = 0;
D3D12_GetParameterBuffer(frame, &paramBuff, &paramBuffSize);
assert(sizeof(GlobalParams) < paramBuffSize);
memcpy(paramBuff, &GlobalParams, sizeof(GlobalParams));

//for every material
//D3D12_SetPipelineState(material.Pipeline)
//  for every material instance
//  D3D12_SetParameterBufferOffset(MATERIAL_PARMS, material.ParamOffset)
//    for every object
//    D3D12_SetParameterBufferOffset(OBJECT_PARMS, object.ParamOffset)
//    D3D12_DrawMesh(object.Mesh)


D3D12_SubmitFrame(frame);
}

*/

struct D3D12_DeviceContext;

struct D3D12_RenderTarget
{
    ID3D12Resource* Resource;
    D3D12_CPU_DESCRIPTOR_HANDLE Handle;
    D3D12_RESOURCE_STATES CurrentState;
    void Transition(ID3D12GraphicsCommandList* cmdList, D3D12_RESOURCE_STATES after)
    {
        if (CurrentState != after)
        {
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier.Transition.StateBefore = CurrentState;
            barrier.Transition.StateAfter = after;
            barrier.Transition.pResource = Resource;
            cmdList->ResourceBarrier(1, &barrier);
            CurrentState = after;
        }
    }
};

struct D3D12_FrameContext
{
    D3D12_DeviceContext* DeviceContext;
    D3D12_RenderTarget RenderTarget;
    ID3D12Resource* ParameterBuffer;
    ID3D12CommandAllocator* CmdAllocator;
    ID3D12Fence* FrameFence;
    void* ParameterBufferPtr;
    HANDLE FrameEvent;
    UINT64 FenceValue;
    UINT64 FrameIndex;
};

struct D3D12_DeviceContext
{
    ID3D12Device* Device;
    IDXGISwapChain3* SwapChain;
    ID3D12CommandQueue* DrawCmdQueue;
    ID3D12CommandQueue* CopyCmdQueue;
    ID3D12DescriptorHeap* RTVDescrHeap;
    ID3D12GraphicsCommandList* CmdList;
    ID3D12RootSignature* RootSignature;
    UINT64 CurrentFrame;
    D3D12_FrameContext FrameContext[FRAME_BUFFERS];
    D3D12_VIEWPORT Viewport;
    D3D12_RECT ScissorRect;

    struct
    {
        unsigned int RTV = 0;
        unsigned int SRV = 0;
        unsigned int DSV = 0;
        unsigned int SMP = 0;
    } DescrSize;

    bool Init(HWND window)
    {
        IDXGIFactory4* factory = nullptr;
        if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))
        {
            return false;
        }
        unsigned int adapterIndex = 0;
        IDXGIAdapter1* adapter = nullptr;
        while (factory->EnumAdapters1(adapterIndex, &adapter) != DXGI_ERROR_NOT_FOUND)
        {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);
            if ((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0)
            {
                break;
            }
            adapter = nullptr;
            adapterIndex++;
        }
        if (adapter == nullptr)
        {
            return false;
        }
#ifdef _DEBUG
        ID3D12Debug *debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();
            debugController->Release();
        }
#endif
        HRESULT hr = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&Device));
        RELEASE(adapter);
        if (FAILED(hr))
        {
            RELEASE(factory);
            return false;
        }
        DescrSize.RTV = Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        DescrSize.DSV = Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
        DescrSize.SRV = Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        DescrSize.SMP = Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
        D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
        if (FAILED(Device->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(&DrawCmdQueue))))
        {
            return false;
        }
        cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
        if (FAILED(Device->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(&CopyCmdQueue))))
        {
            return false;
        }
        RECT windowRect;
        DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
        GetClientRect(static_cast<HWND>(window), &windowRect);
        swapChainDesc.BufferDesc.Width = windowRect.right - windowRect.left;
        swapChainDesc.BufferDesc.Height = windowRect.bottom - windowRect.top;
        swapChainDesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
        swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.BufferCount = FRAME_BUFFERS;
        swapChainDesc.OutputWindow = window;
        swapChainDesc.Windowed = TRUE;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        IDXGISwapChain* swapChain = nullptr;
        hr = factory->CreateSwapChain(DrawCmdQueue, &swapChainDesc, &swapChain);
        RELEASE(factory);
        if (FAILED(hr))
        {
            return false;
        }
        SwapChain = static_cast<IDXGISwapChain3*>(swapChain);
        CurrentFrame = SwapChain->GetCurrentBackBufferIndex();
        D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
        descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        descHeapDesc.NumDescriptors = FRAME_BUFFERS;
        if (FAILED(Device->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(&RTVDescrHeap))))
        {
            return false;
        }
        for (unsigned int i = 0; i < FRAME_BUFFERS; i++)
        {
            D3D12_FrameContext& fc = FrameContext[i];
            if (FAILED(SwapChain->GetBuffer(i, IID_PPV_ARGS(&fc.RenderTarget.Resource))))
            {
                return false;
            }
            fc.RenderTarget.Resource->SetName(L"ScreenColorTarget");
            fc.RenderTarget.Handle = RTVDescrHeap->GetCPUDescriptorHandleForHeapStart();
            fc.RenderTarget.Handle.ptr += i * DescrSize.RTV;
            fc.RenderTarget.CurrentState = D3D12_RESOURCE_STATE_PRESENT;
            Device->CreateRenderTargetView(fc.RenderTarget.Resource, nullptr, fc.RenderTarget.Handle);
            if (FAILED(Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&fc.CmdAllocator))))
            {
                return false;
            }
            if (FAILED(Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fc.FrameFence))))
            {
                return false;
            }
            fc.ParameterBuffer = CreateBuffer(PARAMETER_BUFFER_SIZE, D3D12_HEAP_TYPE_UPLOAD);
            if (!fc.ParameterBuffer)
            {
                return false;
            }
            fc.ParameterBuffer->Map(0, nullptr, &fc.ParameterBufferPtr);
            fc.FrameEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
            fc.DeviceContext = this;
            fc.FenceValue = 0;
        }
        D3D12_FrameContext& fc = FrameContext[CurrentFrame];
        if (FAILED(Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, fc.CmdAllocator, nullptr, IID_PPV_ARGS(&CmdList))))
        {
            return false;
        }
        else
        {
            CmdList->Close();
        }
        D3D12_ROOT_PARAMETER globalParams = {};
        globalParams.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        globalParams.Descriptor.RegisterSpace = 0;
        globalParams.Descriptor.ShaderRegister = 0;
        globalParams.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        D3D12_ROOT_SIGNATURE_DESC rsDescr = {};
        rsDescr.pParameters = &globalParams;
        rsDescr.NumParameters = 1;
        rsDescr.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
        rsDescr.Flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;
        rsDescr.Flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;
        rsDescr.Flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
        ID3DBlob *rsData = nullptr, *rsErrorData = nullptr;
        if (FAILED(D3D12SerializeRootSignature(&rsDescr, D3D_ROOT_SIGNATURE_VERSION_1, &rsData, &rsErrorData)))
        {
            RELEASE(rsErrorData);
            return false;
        }
        if (FAILED(Device->CreateRootSignature(0, rsData->GetBufferPointer(), rsData->GetBufferSize(), IID_PPV_ARGS(&RootSignature))))
        {
            RELEASE(rsErrorData);
            return false;
        }
        RELEASE(rsData);
        Viewport.Width = static_cast<float>(swapChainDesc.BufferDesc.Width);
        Viewport.Height = static_cast<float>(swapChainDesc.BufferDesc.Height);
        Viewport.TopLeftX = 0.f;
        Viewport.TopLeftY = 0.f;
        Viewport.MinDepth = 1.f;
        Viewport.MaxDepth = 0.f;
        ScissorRect = windowRect;
        return true;
    }

    ID3D12Resource* CreateBuffer(size_t bytes, D3D12_HEAP_TYPE type)
    {
        D3D12_RESOURCE_DESC descr = {};
        descr.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        descr.Width = bytes;
        descr.Height = 1;
        descr.DepthOrArraySize = 1;
        descr.MipLevels = 1;
        descr.Format = DXGI_FORMAT_UNKNOWN;
        descr.SampleDesc.Count = 1;
        descr.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = type;
        D3D12_RESOURCE_STATES initState = (type == D3D12_HEAP_TYPE_UPLOAD) ? D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_COPY_DEST;
        ID3D12Resource *result = nullptr;
        if (FAILED(Device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &descr, initState, nullptr, IID_PPV_ARGS(&result))))
        {
            result = nullptr;
        }
        return result;
    }

    UINT64 WaitForLastFrame()
    {
        D3D12_FrameContext& fc = FrameContext[CurrentFrame];
        UINT64 completed = fc.FrameFence->GetCompletedValue();
        if (completed < fc.FenceValue)
        {
            UINT64 next = fc.FenceValue;
            if (FAILED(fc.FrameFence->SetEventOnCompletion(next, fc.FrameEvent)))
            {
                return FRAME_BUFFERS;
            }
            WaitForSingleObject(fc.FrameEvent, INFINITE);
        }
        CurrentFrame = SwapChain->GetCurrentBackBufferIndex();
        ++fc.FenceValue;
        return CurrentFrame;
    }

    void SubmitFrame(D3D12_FrameContext& fc)
    {
        fc.RenderTarget.Transition(CmdList, D3D12_RESOURCE_STATE_PRESENT);
        assert(SUCCEEDED(CmdList->Close()));
        ID3D12CommandList *cmdLists[] = { CmdList };
        DrawCmdQueue->ExecuteCommandLists(1, cmdLists);
        DrawCmdQueue->Signal(fc.FrameFence, fc.FenceValue);
        SwapChain->Present(0, 0);
    }

    void UseDefaultRenderTarget(D3D12_FrameContext& fc)
    {
        fc.RenderTarget.Transition(CmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
        CmdList->RSSetViewports(1, &Viewport);
        CmdList->RSSetScissorRects(1, &ScissorRect);
        CmdList->OMSetRenderTargets(1, &fc.RenderTarget.Handle, false, nullptr);
    }

    void UseRenderTarget(D3D12_RenderTarget& rt)
    {
    }
};

//// API FUNCTIONS

struct D3D12_DeviceContext* D3D12_InitDeviceContext(struct D3D12_DeviceContext** outDC, void* window)
{
    *outDC = NULL;
    D3D12_DeviceContext* dc = new D3D12_DeviceContext();
    if (dc && dc->Init((HWND)window))
    {
        *outDC = dc;
    }
    return *outDC;
}

void D3D12_GetParameterBuffer(struct D3D12_FrameContext* fc, void** outPtr, uint64_t* outMax)
{
    *outPtr = fc->ParameterBufferPtr;
    *outMax = PARAMETER_BUFFER_SIZE;
}

struct D3D12_FrameContext* D3D12_PrepareFrame(struct D3D12_DeviceContext* dc, struct D3D12_FrameContext** outFC)
{
    *outFC = NULL;
    UINT64 idx = FRAME_BUFFERS;
    assert((idx = dc->WaitForLastFrame()) < FRAME_BUFFERS);
    D3D12_FrameContext* fc = &dc->FrameContext[idx];
    assert(SUCCEEDED(fc->CmdAllocator->Reset()));
    assert(SUCCEEDED(dc->CmdList->Reset(fc->CmdAllocator, nullptr)));
    dc->CmdList->SetGraphicsRootSignature(dc->RootSignature);
    *outFC = fc;
    return *outFC;
}

void D3D12_BeginRenderPass(struct D3D12_FrameContext* fc, struct RenderPass* pass)
{
    D3D12_DeviceContext& dc = *fc->DeviceContext;
    (pass->OutputTarget) ? dc.UseRenderTarget(*pass->OutputTarget) : dc.UseDefaultRenderTarget(*fc);
    if (pass->Flags & FL_PASS_CLEAR_COLOR)
    {
        dc.CmdList->ClearRenderTargetView(fc->RenderTarget.Handle, &pass->ClearColor.R, 0, nullptr);
    }
}

void D3D12_CommitFrame(struct D3D12_FrameContext* fc)
{
    assert(fc->DeviceContext);
    fc->DeviceContext->SubmitFrame(*fc);
}

#endif
