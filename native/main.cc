#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <DirectXMath.h>
#include <cstdio>
#include <cmath>
#include <dxgi1_2.h>

// 全局变量
HWND hWnd = nullptr;
ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pImmediateContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;

// 共享纹理（核心）
ID3D11Texture2D* g_pSharedTexture = nullptr;
HANDLE g_SharedHandle = nullptr;

// 窗口尺寸
const UINT WIDTH = 400;
const UINT HEIGHT = 400;

// 窗口过程
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

// 初始化窗口
bool InitWindow(HINSTANCE hInstance)
{
    WNDCLASSEX wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"D3D11SharedTextureProducer";

    if (!RegisterClassEx(&wc))
        return false;

    hWnd = CreateWindowEx(
        0,
        L"D3D11SharedTextureProducer",
        L"进程A - 共享纹理生产者（绘制+输出句柄）",
        WS_OVERLAPPEDWINDOW,
        100, 100, WIDTH, HEIGHT,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!hWnd)
        return false;

    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);
    return true;
}

// 初始化Direct3D11 + 创建共享纹理
bool InitD3D11()
{
    HRESULT hr;

    // 1. 创建D3D11设备和交换链
    DXGI_SWAP_CHAIN_DESC scDesc = { 0 };
    scDesc.BufferCount = 1;
    scDesc.BufferDesc.Width = WIDTH;
    scDesc.BufferDesc.Height = HEIGHT;
    scDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scDesc.BufferDesc.RefreshRate.Numerator = 60;
    scDesc.BufferDesc.RefreshRate.Denominator = 1;
    scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scDesc.OutputWindow = hWnd;
    scDesc.SampleDesc.Count = 1;
    scDesc.SampleDesc.Quality = 0;
    scDesc.Windowed = TRUE;

    hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION,
        &scDesc, &g_pSwapChain, &g_pd3dDevice, nullptr, &g_pImmediateContext
    );
    if (FAILED(hr)) return false;

    // 2. 创建渲染目标
    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
    pBackBuffer->Release();
    g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);

    // 3. 设置视口
    D3D11_VIEWPORT vp = { 0 };
    vp.Width = (FLOAT)WIDTH;
    vp.Height = (FLOAT)HEIGHT;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    g_pImmediateContext->RSSetViewports(1, &vp);

    // ==============================================
    // 核心：创建 可共享的纹理
    // ==============================================
    D3D11_TEXTURE2D_DESC texDesc = { 0 };
    texDesc.Width = WIDTH;
    texDesc.Height = HEIGHT;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    
    // 关键标记：允许跨进程共享
    texDesc.MiscFlags =  D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED;

    hr = g_pd3dDevice->CreateTexture2D(&texDesc, nullptr, &g_pSharedTexture);
    if (FAILED(hr)) return false;

    // 从共享纹理获取共享句柄（给进程B使用）
    IDXGIResource1* pDXGIResource = nullptr;
    g_pSharedTexture->QueryInterface(__uuidof(IDXGIResource1), (LPVOID*)&pDXGIResource);
    //pDXGIResource->GetSharedHandle(&g_SharedHandle);
    pDXGIResource->CreateSharedHandle(nullptr, GENERIC_ALL, nullptr, &g_SharedHandle);
    pDXGIResource->Release();

    // ==============================================
    // 打印共享句柄（进程B需要这个值！）
    // ==============================================
    printf("=========================================\n");
    printf("进程A：共享纹理创建成功！\n");
    printf("共享句柄值：0x%p\n", g_SharedHandle);
    printf("请复制此句柄给进程B使用\n");
    printf("=========================================\n");

    return true;
}

// 渲染：绘制动态彩色图形到共享纹理
void Render()
{
    // 清屏颜色（动态变化）
    static float t = 0.0f;
    t += 0.01f;
    FLOAT clearColor[4] = {
        (sinf(t) + 1.0f) * 0.5f,    // R
        (sinf(t + 2.0f) + 1.0f) * 0.5f, // G
        (sinf(t + 4.0f) + 1.0f) * 0.5f, // B
        1.0f
    };

    // 1. 渲染到共享纹理
    ID3D11RenderTargetView* pSharedRTV = nullptr;
    g_pd3dDevice->CreateRenderTargetView(g_pSharedTexture, nullptr, &pSharedRTV);
    g_pImmediateContext->OMSetRenderTargets(1, &pSharedRTV, nullptr);
    g_pImmediateContext->ClearRenderTargetView(pSharedRTV, clearColor);
    pSharedRTV->Release();

    // 2. 同时渲染到自己窗口显示
    g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);
    g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, clearColor);

    // 提交画面
    g_pSwapChain->Present(1, 0);
}

// 释放资源
void Cleanup()
{
    if (g_pSharedTexture) g_pSharedTexture->Release();
    if (g_pRenderTargetView) g_pRenderTargetView->Release();
    if (g_pSwapChain) g_pSwapChain->Release();
    if (g_pImmediateContext) g_pImmediateContext->Release();
    if (g_pd3dDevice) g_pd3dDevice->Release();
    UnregisterClass(L"D3D11SharedTextureProducer", GetModuleHandle(nullptr));
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    if (!InitWindow(hInstance)) return 0;
    if (!InitD3D11())
    {
        Cleanup();
        return 0;
    }

    // 主循环
    MSG msg = { 0 };
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            Render();
        }
    }

    Cleanup();
    return 0;
}