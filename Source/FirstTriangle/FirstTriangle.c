#include <windows.h>
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

#include <Render/Renderer.h>

#define APP_NAME L"FirstTriangle"

typedef struct  
{
    void* RawMemory;
    RenderPass* FirstPass;
    RenderPass* LastPass;
    RenderPass BasePass;
} RendererScene;

void CreateScene(RendererDevice dc, RendererScene** scene)
{
    //start scene
    void* mem = malloc(65536);
    RendererScene* myScene = (RendererScene*)mem;
    myScene->RawMemory = myScene + 1;
    myScene->FirstPass = NULL;
    myScene->LastPass = NULL;

    //add a pass
    {
        RenderPass* newPass = (RenderPass*)myScene->RawMemory;
        myScene->RawMemory = newPass + 1;
        newPass->Name = L"BasePass";
        newPass->OutputTarget = DEFAULT_RENDER_TARGET;
        newPass->ClearColor.R = 0.3922f;
        newPass->ClearColor.G = 0.5843f;
        newPass->ClearColor.B = 0.9294f;
        newPass->ClearColor.A = 1.f;
        newPass->Next = NULL;
        newPass->Flags = FL_PASS_CLEAR_COLOR;
        if (!myScene->FirstPass)
        {
            myScene->FirstPass = newPass;
        }
        if (myScene->LastPass)
        {
            myScene->LastPass->Next = newPass;
        }
        myScene->LastPass = newPass;
    }

    //add a pass
    {
        RenderPass* newPass = (RenderPass*)myScene->RawMemory;
        myScene->RawMemory = newPass + 1;
        newPass->Name = L"UIPass";
        newPass->OutputTarget = DEFAULT_RENDER_TARGET;
        newPass->ClearColor.R = 1.f;
        newPass->ClearColor.G = 1.f;
        newPass->ClearColor.B = 0.f;
        newPass->ClearColor.A = 1.f;
        newPass->Next = NULL;
        newPass->Flags = 0;
        if (!myScene->FirstPass)
        {
            myScene->FirstPass = newPass;
        }
        if (myScene->LastPass)
        {
            myScene->LastPass->Next = newPass;
        }
        myScene->LastPass = newPass;
    }

    *scene = myScene;
}

void RenderScene(RendererDevice dc, RendererFrame fc, RendererScene* scene)
{
    void *paramBuff = NULL;
    uint64_t paramBuffSize = 0;
    GetParameterBuffer(fc, &paramBuff, &paramBuffSize);
    for (RenderPass* pass = scene->FirstPass; pass; pass = pass->Next)
    {
        BeginRenderPass(fc, pass);
        EndRenderPass(fc, pass);
    }
}

LRESULT WINAPI WndProc(HWND wnd, UINT msg, WPARAM w, LPARAM l)
{
    LRESULT retVal = 0;
    switch (msg)
    {
    case WM_SYSCOMMAND:
        if (w == SC_CLOSE)
        {
            PostQuitMessage(0);
        }
        break;
    default:
        retVal = DefWindowProc(wnd, msg, w, l);
        break;
    }
    return retVal;
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show)
{
    static WNDCLASS windowClass;
    windowClass.style = CS_OWNDC;
    windowClass.lpfnWndProc = (WNDPROC)WndProc;
    windowClass.hInstance = GetModuleHandle(NULL);
    windowClass.hIcon = LoadIcon(NULL, IDI_WINLOGO);
    windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    windowClass.lpszClassName = L"WNDCLASS_D3D12DEMO";
    if (!RegisterClass(&windowClass))
    {
        return show;
    }
    MONITORINFO monitorInfo = { sizeof(MONITORINFO) };
    HMONITOR monitor = MonitorFromWindow(NULL, MONITOR_DEFAULTTONEAREST);
    GetMonitorInfo(monitor, &monitorInfo);
#if FULLSCREEN
    DWORD style = WS_POPUP | WS_VISIBLE;
    int rows = monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top;
    int cols = monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left;
    int left = monitorInfo.rcMonitor.left;
    int top = monitorInfo.rcMonitor.top;
#else
    RECT rect;
    SetRect(&rect, 0, 0, 1024, 768);
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_VISIBLE;
    AdjustWindowRect(&rect, style, FALSE);
    int rows = rect.bottom - rect.top;
    int cols = rect.right - rect.left;
    int left = ((monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left) - cols) >> 1;
    int top = ((monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top) - rows) >> 1;
#endif
    LPCWSTR title = L"D3D12DEMO_" APP_NAME;
    HWND window = CreateWindow(windowClass.lpszClassName, title, style, left, top, cols, rows, NULL, NULL, inst, NULL);
    if (!window)
    {
        return show;
    }
    MSG message;
    RendererDevice dc;
    bool running = true;
    RendererScene* scene = NULL;
    assert(RendererInitDevice(&dc, window));
    CreateScene(dc, &scene);
    while (scene && running)
    {
        while (PeekMessage(&message, window, 0, 0, PM_REMOVE))
        {
            RendererFrame fc;
            RendererInitFrame(dc, &fc);
            RenderScene(dc, fc, scene);
            RendererCommitFrame(fc);
            TranslateMessage(&message);
            DispatchMessage(&message);
            if (message.message == WM_QUIT)
            {
                DestroyWindow(window);
                running = false;
                break;
            }
        }
    }
    return show;
}