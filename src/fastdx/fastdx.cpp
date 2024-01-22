#include "fastdx.h"
using namespace fastdx;
using namespace Microsoft::WRL;

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if (p) { (p)->Release(); (p)=nullptr; } }
#endif

#ifndef RETURN_FAILED_VOID
#define RETURN_FAILED_VOID(HRESULTVAR, OUTVAR) { if (_checkFailedAndAssign(HRESULTVAR, OUTVAR)) { return; } }
#endif

#ifndef RETURN_FAILED
#define RETURN_FAILED(HRESULTVAR, OUTVAR) { if (_checkFailedAndAssign(HRESULTVAR, OUTVAR)) { return nullptr; } }
#endif

#ifndef HRESULT_OK
#define HRESULT_OK(OUTVAR) { if (OUTVAR) { *(OUTVAR) = S_OK; } }
#endif

struct ComReleaser {
    void operator() (IUnknown* ptr) {
        SAFE_RELEASE(ptr);
    }
};

bool _checkFailedAndAssign(HRESULT hr, HRESULT* outResult) {
    if (FAILED(hr) && outResult != nullptr) {
        *outResult = hr;
    }
    return FAILED(hr);
}

namespace fastdx {
    HWND hwnd = nullptr;
    std::shared_ptr<IDXGIFactory4> _dxgiFactory;
};


std::shared_ptr<IDXGIFactory4> getOrCreateDXIG(HRESULT* outResult) {
    if (_dxgiFactory) {
        HRESULT_OK(outResult)
        return _dxgiFactory;
    }

    uint32_t dxgiFactoryFlags = 0;

#if defined(_DEBUG)
    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
        dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
    }
#endif

    HRESULT hr;
    ComPtr<IDXGIFactory4> dxgiFactory;
    hr = CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&dxgiFactory));
    if (SUCCEEDED(hr)) {
        _dxgiFactory = std::shared_ptr<IDXGIFactory4>(dxgiFactory.Detach(), ComReleaser());
    }

    HRESULT_OK(outResult)
    return _dxgiFactory;
}


ID3D12Device2Ptr fastdx::createDevice(D3D_FEATURE_LEVEL featureLevel, HRESULT* outResult) {
    HRESULT hr;
    std::shared_ptr<IDXGIFactory4> dxgiFactory = getOrCreateDXIG(&hr);

    ID3D12Device2* device = nullptr;
    ComPtr<IDXGIAdapter1> hardwareAdapter;
    for (int32_t i = 0; dxgiFactory->EnumAdapters1(i, &hardwareAdapter) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC1 desc;
        hardwareAdapter->GetDesc1(&desc);

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
            continue;
        }

        HRESULT hr = D3D12CreateDevice(hardwareAdapter.Get(), featureLevel, IID_PPV_ARGS(&device));
        if (SUCCEEDED(hr)) {
            break;
        }
    }

    HRESULT_OK(outResult);
    return ID3D12Device2Ptr(device, ComReleaser());
}


ID3D12CommandQueuePtr fastdx::createCommandQueue(ID3D12Device2Ptr device, D3D12_COMMAND_LIST_TYPE type, HRESULT* outResult) {
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = type;

    ID3D12CommandQueue* commandQueue = nullptr;
    HRESULT hr = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue));
    RETURN_FAILED(hr, outResult);

    return ID3D12CommandQueuePtr(commandQueue, ComReleaser());
}


IDXGISwapChain3Ptr fastdx::createWindowSwapChain(ID3D12Device2Ptr device, ID3D12CommandQueuePtr commandQueue, DXGI_FORMAT format, HRESULT* outResult) {
    HRESULT hr;
    std::shared_ptr<IDXGIFactory4> dxgiFactory = getOrCreateDXIG(&hr);
    RETURN_FAILED(hr, outResult);

    RECT windowRect;
    GetWindowRect(hwnd, &windowRect);

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = 3; // Tripple Buffering
    swapChainDesc.Width = windowRect.right -windowRect.left;
    swapChainDesc.Height = windowRect.bottom -windowRect.top;
    swapChainDesc.Format = format;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swapChain1;
    hr = dxgiFactory->CreateSwapChainForHwnd(
        commandQueue.get(),     // Swap chain is linked to queue
        hwnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain1
    );
    RETURN_FAILED(hr, outResult);

    hr = dxgiFactory->MakeWindowAssociation(hwnd, 0);
    RETURN_FAILED(hr, outResult);

    ComPtr<IDXGISwapChain3> swapChain3;
    hr = swapChain1.As(&swapChain3);
    RETURN_FAILED(hr, outResult);

    HRESULT_OK(outResult);
    return IDXGISwapChain3Ptr(swapChain3.Detach(), ComReleaser());
}


ID3D12DescriptorHeapPtr fastdx::createHeapDescriptor(ID3D12Device2Ptr device,
    int32_t count, D3D12_DESCRIPTOR_HEAP_TYPE heapType, HRESULT* outResult) {

    D3D12_DESCRIPTOR_HEAP_FLAGS heapFlags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    if (heapType == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV || heapType == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER) {
        heapFlags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    }

    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = count;
    rtvHeapDesc.Type = heapType;
    rtvHeapDesc.Flags = heapFlags;

    HRESULT hr;
    ComPtr<ID3D12DescriptorHeap> heapDescriptor;
    hr = device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&heapDescriptor));
    RETURN_FAILED(hr, outResult);

    return ID3D12DescriptorHeapPtr(heapDescriptor.Detach(), ComReleaser());
}


void fastdx::createRenderTargetViews(ID3D12Device2Ptr device,
    IDXGISwapChain3Ptr swapChain, ID3D12DescriptorHeapPtr heap, HRESULT* outResult) {

    HRESULT hr;
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    hr = swapChain->GetDesc1(&swapChainDesc);
    RETURN_FAILED_VOID(hr, outResult);

    D3D12_CPU_DESCRIPTOR_HANDLE heapHandle = heap->GetCPUDescriptorHandleForHeapStart();
    size_t heapDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    ComPtr<ID3D12Resource> renderTarget;
    for (uint32_t i = 0; i < swapChainDesc.BufferCount; ++i) {
        hr = swapChain->GetBuffer(i++, IID_PPV_ARGS(&renderTarget));
        RETURN_FAILED_VOID(hr, outResult);

        device->CreateRenderTargetView(renderTarget.Get(), nullptr, heapHandle);
        heapHandle.ptr += heapDescriptorSize;
    }
}


LRESULT CALLBACK WindowProcKeyDown(HWND hWnd, WPARAM wParam, LPARAM lParam) {
    switch (wParam) {
        case VK_ESCAPE:
            SendMessage(hWnd, WM_CLOSE, 0, 0);
            break;
    }

    return S_OK;
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT:
            break;

        case WM_SIZE:
            break;

        case WM_KEYDOWN:
            WindowProcKeyDown(hWnd, wParam, lParam);
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

HRESULT fastdx::createWindow(const WindowProperties& properties, HWND* optOutWindow) {
    HINSTANCE hInstance = (HINSTANCE)GetModuleHandle(nullptr);

    WNDCLASSEX wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = properties.isFullScreen? 0 : CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = (WNDPROC)WindowProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"fastdx";
    RegisterClassEx(&wc);

    hwnd = CreateWindow(
        wc.lpszClassName,
        properties.title,
        properties.isFullScreen? WS_POPUP : WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        properties.width,
        properties.height,
        nullptr,
        nullptr,
        hInstance,
        0);

    if (!hwnd) {
        HRESULT result = HRESULT_FROM_WIN32(GetLastError());
        UnregisterClass(wc.lpszClassName, nullptr);
        return result;
    }

    if (optOutWindow) {
        *optOutWindow = hwnd;
    }

    ShowWindow(hwnd, properties.showMode);

    return S_OK;
}

HRESULT fastdx::runMainLoop() {
    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return S_OK;
}