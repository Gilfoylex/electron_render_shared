#include <windows.h>
#include <d3d11.h>
#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <string>

HWND hWnd = nullptr;
ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pImmediateContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;

ID3D11Texture2D* g_pSharedTexture = nullptr;
ID3D11ShaderResourceView* g_pSharedSRV = nullptr;

ID3D11VertexShader* g_pVertexShader = nullptr;
ID3D11PixelShader* g_pPixelShader = nullptr;
ID3D11InputLayout* g_pInputLayout = nullptr;
ID3D11Buffer* g_pVertexBuffer = nullptr;
ID3D11SamplerState* g_pSampler = nullptr;

struct SimpleVertex {
    float pos[3];
    float uv[2];
};

const UINT WIDTH = 400;
const UINT HEIGHT = 400;

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_DESTROY) PostQuitMessage(0);
    else return DefWindowProc(hWnd, msg, wParam, lParam);
    return 0;
}

bool InitWindow(HINSTANCE hInstance)
{
    WNDCLASSEX wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"D3D11Consumer";

    if (!RegisterClassEx(&wc)) return false;

    hWnd = CreateWindowEx(0, L"D3D11Consumer", L"进程B - 共享纹理显示",
        WS_OVERLAPPEDWINDOW, 200, 200, WIDTH, HEIGHT,
        nullptr, nullptr, hInstance, nullptr);

    if (!hWnd) return false;
    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);
    return true;
}

ID3DBlob* CompileShader(const char* code, const char* entry, const char* target)
{
    ID3DBlob* blob = nullptr;
    ID3DBlob* err = nullptr;
    D3DCompile(code, strlen(code), nullptr, nullptr, nullptr, entry, target, 0, 0, &blob, &err);
    if (err) printf("着色器错误：%s\n", (char*)err->GetBufferPointer());
    return blob;
}

bool InitFullScreenShader()
{
    const char* vsCode = R"(
        struct VSInput { float3 pos : POSITION; float2 uv : TEXCOORD0; };
        struct PSInput { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };
        PSInput VS(VSInput input) {
            PSInput o;
            o.pos = float4(input.pos, 1.0f);
            o.uv = input.uv;
            return o;
        }
    )";

    const char* psCode = R"(
        Texture2D tex : register(t0);
        SamplerState samp : register(s0);
        float4 PS(float4 pos : SV_POSITION, float2 uv : TEXCOORD0) : SV_Target {
            return tex.Sample(samp, uv);
        }
    )";

    ID3DBlob* vsBlob = CompileShader(vsCode, "VS", "vs_5_0");
    ID3DBlob* psBlob = CompileShader(psCode, "PS", "ps_5_0");

    g_pd3dDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &g_pVertexShader);
    g_pd3dDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &g_pPixelShader);

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    g_pd3dDevice->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &g_pInputLayout);

    SimpleVertex vertices[] = {
        {{-1.0f,  1.0f, 0}, {0, 0}},
        {{ 1.0f,  1.0f, 0}, {1, 0}},
        {{-1.0f, -1.0f, 0}, {0, 1}},
        {{ 1.0f, -1.0f, 0}, {1, 1}},
    };

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.ByteWidth = sizeof(vertices);
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.Usage = D3D11_USAGE_IMMUTABLE;

    D3D11_SUBRESOURCE_DATA data = {};
    data.pSysMem = vertices;
    g_pd3dDevice->CreateBuffer(&vbDesc, &data, &g_pVertexBuffer);

    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    g_pd3dDevice->CreateSamplerState(&sampDesc, &g_pSampler);

    vsBlob->Release();
    psBlob->Release();
    return true;
}

bool InitD3D11()
{
    DXGI_SWAP_CHAIN_DESC desc = {};
    desc.BufferCount = 1;
    desc.BufferDesc.Width = WIDTH;
    desc.BufferDesc.Height = HEIGHT;
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.OutputWindow = hWnd;
    desc.SampleDesc.Count = 1;
    desc.Windowed = TRUE;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION,
        &desc, &g_pSwapChain, &g_pd3dDevice, nullptr, &g_pImmediateContext
    );

    ID3D11Texture2D* backBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    g_pd3dDevice->CreateRenderTargetView(backBuffer, nullptr, &g_pRenderTargetView);
    backBuffer->Release();

    g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);

    D3D11_VIEWPORT vp = {};
    vp.Width = WIDTH;
    vp.Height = HEIGHT;
    g_pImmediateContext->RSSetViewports(1, &vp);

    InitFullScreenShader();
    return true;
}

bool OpenSharedTexture(HANDLE hShared)
{
    // 关键修复：使用 ID3D11Device1::OpenSharedResource1() 而不是 OpenSharedResource()
    // 因为进程A使用了 D3D11_RESOURCE_MISC_SHARED_NTHANDLE 标志和 CreateSharedHandle()
    
    ID3D11Device1* pDevice1 = nullptr;
    HRESULT hr = g_pd3dDevice->QueryInterface(__uuidof(ID3D11Device1), (LPVOID*)&pDevice1);
    
    if (FAILED(hr)) {
        printf("❌ 获取 ID3D11Device1 接口失败！错误码: 0x%08X\n", hr);
        printf("   需要 Windows 8.1+ 系统\n");
        return false;
    }

    // 使用 OpenSharedResource1 打开 NT HANDLE 类型的共享资源
    hr = pDevice1->OpenSharedResource1(hShared, __uuidof(ID3D11Texture2D), (LPVOID*)&g_pSharedTexture);
    pDevice1->Release();
    
    if (FAILED(hr)) {
        printf("❌ 打开共享纹理失败！错误码: 0x%08X\n", hr);
        printf("   可能原因：\n");
        printf("   - 句柄无效或已过期\n");
        printf("   - 进程权限不足\n");
        printf("   - 共享资源已被释放\n");
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    hr = g_pd3dDevice->CreateShaderResourceView(g_pSharedTexture, &srvDesc, &g_pSharedSRV);
    if (FAILED(hr)) return false;

    printf("✅ 共享纹理连接成功！\n");
    return true;
}

void Render()
{
    if (!g_pSharedSRV) return;

    float black[] = { 0,0,0,1 };
    g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, black);

    UINT stride = sizeof(SimpleVertex);
    UINT offset = 0;
    g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
    g_pImmediateContext->IASetInputLayout(g_pInputLayout);
    g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    g_pImmediateContext->VSSetShader(g_pVertexShader, nullptr, 0);
    g_pImmediateContext->PSSetShader(g_pPixelShader, nullptr, 0);
    g_pImmediateContext->PSSetSamplers(0, 1, &g_pSampler);
    g_pImmediateContext->PSSetShaderResources(0, 1, &g_pSharedSRV);

    g_pImmediateContext->Draw(4, 0);
    g_pSwapChain->Present(1, 0);
}

void Cleanup()
{
    if (g_pSharedSRV) g_pSharedSRV->Release();
    if (g_pSharedTexture) g_pSharedTexture->Release();
    if (g_pRenderTargetView) g_pRenderTargetView->Release();
    if (g_pSwapChain) g_pSwapChain->Release();
    if (g_pImmediateContext) g_pImmediateContext->Release();
    if (g_pd3dDevice) g_pd3dDevice->Release();
    if (g_pSampler) g_pSampler->Release();
    if (g_pVertexBuffer) g_pVertexBuffer->Release();
    if (g_pInputLayout) g_pInputLayout->Release();
    if (g_pVertexShader) g_pVertexShader->Release();
    if (g_pPixelShader) g_pPixelShader->Release();
    UnregisterClass(L"D3D11Consumer", GetModuleHandle(nullptr));
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    if (!InitWindow(hInstance) || !InitD3D11()) { Cleanup(); return 0; }

    HANDLE hShared = (HANDLE)strtoul("0x0000000000000510", nullptr, 16);

    // 获取 A 进程的句柄（需要知道 A 进程的 PID）
    DWORD dwSourceProcessId = 50348;  // 替换为 A 进程的实际 PID
    HANDLE hSourceProcess = OpenProcess(PROCESS_DUP_HANDLE, FALSE, dwSourceProcessId);
    if (!hSourceProcess) {
        printf("❌ 打开源进程失败！错误码：%lu\n", GetLastError());
        Cleanup();
        return 0;
    }

    // 复制句柄到当前进程
    HANDLE hDuplicatedHandle = nullptr;
    BOOL bSuccess = DuplicateHandle(
        hSourceProcess,           // 源进程句柄
        hShared,                  // 源句柄
        GetCurrentProcess(),      // 目标进程（当前进程）
        &hDuplicatedHandle,       // 输出：复制后的句柄
        0,                        // 访问权限（0 表示使用源句柄的权限）
        FALSE,                    // 不继承
        DUPLICATE_SAME_ACCESS     // 复制相同的访问权限
    );

    CloseHandle(hSourceProcess);  // 关闭源进程句柄

    if (!bSuccess) {
        printf("❌ DuplicateHandle 失败！错误码：%lu\n", GetLastError());
        Cleanup();
        return 0;
    }

    printf("✅ 句柄复制成功！\n");

    if (!OpenSharedTexture(hDuplicatedHandle)) { Cleanup(); return 0; }

    MSG msg = { 0 };
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            Render();
        }
    }

    Cleanup();
    return 0;
}