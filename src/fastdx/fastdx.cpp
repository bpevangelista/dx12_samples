#include "fastdx.h"

#include <chrono>
#include <dxgidebug.h>
#include <wrl.h> // Windows Runtime Library

using namespace std::chrono;
using namespace fastdx;
using namespace Microsoft::WRL;

#ifndef CHECK_ASSIGN_RETURN_IF_FAILED_
#define CHECK_ASSIGN_RETURN_IF_FAILED_(HRESULTVAR, OUTVAR, ...) { if (_checkFailedAndAssign(HRESULTVAR, OUTVAR)) { return __VA_ARGS__; } }
#endif

#ifndef CHECK_ASSIGN_RETURN_IF_FAILED
#define CHECK_ASSIGN_RETURN_IF_FAILED(HRESULTVAR, OUTVAR) { if (_checkFailedAndAssign(HRESULTVAR, OUTVAR)) { return nullptr; } }
#endif


bool _checkFailedAndAssign(HRESULT hr, HRESULT* outResult) {
    if (outResult != nullptr) {
        *outResult = hr;
    }
    return FAILED(hr);
}

namespace fastdx {
    std::shared_ptr<IDXGIFactory4> _dxgiFactory = nullptr;
};


std::shared_ptr<IDXGIFactory4> getOrCreateDXIG(HRESULT* outResult) {
    if (_dxgiFactory) {
        if (outResult) {
            *outResult = S_OK;
        }
        return _dxgiFactory;
    }

    uint32_t dxgiFactoryFlags = 0;

#if defined(_DEBUG)
    ComPtr<ID3D12Debug1> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
        debugController->SetEnableGPUBasedValidation(true);
        debugController->SetEnableSynchronizedCommandQueueValidation(true);

        dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;

        ComPtr<IDXGIInfoQueue> dxgiInfoQueue;
        if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiInfoQueue)))) {
            dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
            dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);
        }
    }
#endif

    HRESULT hr;
    ComPtr<IDXGIFactory4> dxgiFactory;
    hr = CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&dxgiFactory));
    if (SUCCEEDED(hr)) {
        _dxgiFactory = std::shared_ptr<IDXGIFactory4>(dxgiFactory.Detach(), PtrDeleter());
    }

    CHECK_ASSIGN_RETURN_IF_FAILED(hr, outResult)
    return _dxgiFactory;
}


D3D12DeviceWrapperPtr fastdx::createDevice(D3D_FEATURE_LEVEL featureLevel, HRESULT* outResult) {
    HRESULT hr = E_FAIL;
    std::shared_ptr<IDXGIFactory4> dxgiFactory = getOrCreateDXIG(&hr);

    ID3D12Device2* device = nullptr;
    ComPtr<IDXGIAdapter1> hardwareAdapter;
    for (int32_t i = 0; dxgiFactory->EnumAdapters1(i, &hardwareAdapter) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC1 desc;
        hardwareAdapter->GetDesc1(&desc);

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
            continue;
        }

        hr = D3D12CreateDevice(hardwareAdapter.Get(), featureLevel, IID_PPV_ARGS(&device));
        if (SUCCEEDED(hr)) {
            break;
        }
    }
    CHECK_ASSIGN_RETURN_IF_FAILED(hr, outResult);

    auto devicePtr = std::shared_ptr<ID3D12Device2>(device, PtrDeleter());
    return D3D12DeviceWrapperPtr(new D3D12DeviceWrapper(devicePtr));
}


ID3D12CommandAllocatorPtr D3D12DeviceWrapper::createCommandAllocator(D3D12_COMMAND_LIST_TYPE commandType, HRESULT* outResult) {

    HRESULT hr;
    ComPtr<ID3D12CommandAllocator> commandAllocator;
    hr = _device->CreateCommandAllocator(commandType, IID_PPV_ARGS(&commandAllocator));

    CHECK_ASSIGN_RETURN_IF_FAILED(hr, outResult);
    return ID3D12CommandAllocatorPtr(commandAllocator.Detach(), PtrDeleter());
}


ID3D12GraphicsCommandListPtr D3D12DeviceWrapper::createCommandList(uint32_t nodeMask, D3D12_COMMAND_LIST_TYPE commandType,
    ID3D12CommandAllocatorPtr allocator, HRESULT* outResult) {

    HRESULT hr;
    ComPtr<ID3D12GraphicsCommandList6> commandList;
    hr = _device->CreateCommandList(nodeMask, commandType, allocator.get(), nullptr, IID_PPV_ARGS(&commandList));

    CHECK_ASSIGN_RETURN_IF_FAILED(hr, outResult);
    return ID3D12GraphicsCommandListPtr(commandList.Detach(), PtrDeleter());
}


ID3D12CommandQueuePtr D3D12DeviceWrapper::createCommandQueue(D3D12_COMMAND_LIST_TYPE type, HRESULT* outResult) {
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = type;

    ID3D12CommandQueue* commandQueue = nullptr;
    HRESULT hr = _device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue));

    CHECK_ASSIGN_RETURN_IF_FAILED(hr, outResult);
    return ID3D12CommandQueuePtr(commandQueue, PtrDeleter());
}


ID3D12FencePtr D3D12DeviceWrapper::createFence(uint64_t initialValue, D3D12_FENCE_FLAGS flags, HRESULT* outResult) {

    ID3D12Fence1* fence = nullptr;
    HRESULT hr = _device->CreateFence(initialValue, flags, IID_PPV_ARGS(&fence));

    CHECK_ASSIGN_RETURN_IF_FAILED(hr, outResult);
    return ID3D12FencePtr(fence, PtrDeleter());
}


ID3D12PipelineStatePtr D3D12DeviceWrapper::createGraphicsPipelineState(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc,
    HRESULT* outResult) {
    ID3D12PipelineState* pipelineState = nullptr;
    HRESULT hr = _device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipelineState));

    CHECK_ASSIGN_RETURN_IF_FAILED(hr, outResult);
    return ID3D12PipelineStatePtr(pipelineState, PtrDeleter());
}


ID3D12DescriptorHeapPtr D3D12DeviceWrapper::createHeapDescriptor(int32_t count, D3D12_DESCRIPTOR_HEAP_TYPE heapType,
    HRESULT* outResult) {

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
    hr = _device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&heapDescriptor));

    CHECK_ASSIGN_RETURN_IF_FAILED(hr, outResult);
    return ID3D12DescriptorHeapPtr(heapDescriptor.Detach(), PtrDeleter());
}


std::vector<ID3D12ResourcePtr> D3D12DeviceWrapper::createRenderTargetViews(
    IDXGISwapChainPtr swapChain, ID3D12DescriptorHeapPtr heap, HRESULT* outResult) {

    HRESULT hr;
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    hr = swapChain->GetDesc1(&swapChainDesc);
    CHECK_ASSIGN_RETURN_IF_FAILED_(hr, outResult, std::vector<ID3D12ResourcePtr>());

    D3D12_CPU_DESCRIPTOR_HANDLE heapHandle = heap->GetCPUDescriptorHandleForHeapStart();
    size_t heapDescriptorSize = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    std::vector<ID3D12ResourcePtr> resources;
    ComPtr<ID3D12Resource> renderTarget;
    for (uint32_t i = 0; i < swapChainDesc.BufferCount; ++i) {
        hr = swapChain->GetBuffer(i, IID_PPV_ARGS(&renderTarget));
        CHECK_ASSIGN_RETURN_IF_FAILED_(hr, outResult, std::vector<ID3D12ResourcePtr>());

        _device->CreateRenderTargetView(renderTarget.Get(), nullptr, heapHandle);
        heapHandle.ptr += heapDescriptorSize;
        resources.push_back(ID3D12ResourcePtr(renderTarget.Detach(), PtrDeleter()));
    }

    return resources;
}


ID3D12RootSignaturePtr D3D12DeviceWrapper::createRootSignature(uint32_t nodeMask, const void* data, size_t dataSizeInBytes,
    HRESULT* outResult) {
    ID3D12RootSignature* rootSignature = nullptr;
    HRESULT hr = _device->CreateRootSignature(nodeMask, data, dataSizeInBytes, IID_PPV_ARGS(&rootSignature));

    CHECK_ASSIGN_RETURN_IF_FAILED(hr, outResult);
    return ID3D12RootSignaturePtr(rootSignature, PtrDeleter());

}


IDXGISwapChainPtr D3D12DeviceWrapper::createSwapChainForHwnd(ID3D12CommandQueuePtr commandQueue,
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc, HWND hwnd, HRESULT* outResult) {

    HRESULT hr;
    std::shared_ptr<IDXGIFactory4> dxgiFactory = getOrCreateDXIG(&hr);
    CHECK_ASSIGN_RETURN_IF_FAILED(hr, outResult);

    ComPtr<IDXGISwapChain1> swapChain1;
    hr = dxgiFactory->CreateSwapChainForHwnd(
        commandQueue.get(),     // Swap chain is linked to queue
        hwnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain1
    );
    CHECK_ASSIGN_RETURN_IF_FAILED(hr, outResult);

    hr = dxgiFactory->MakeWindowAssociation(hwnd, 0);
    CHECK_ASSIGN_RETURN_IF_FAILED(hr, outResult);

    ComPtr<IDXGISwapChain3> swapChain3;
    hr = swapChain1.As(&swapChain3);
    CHECK_ASSIGN_RETURN_IF_FAILED(hr, outResult);

    return IDXGISwapChainPtr(swapChain3.Detach(), PtrDeleter());
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


HWND fastdx::createWindow(const WindowProperties& properties, HRESULT* outResult) {
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

    HWND hwnd = CreateWindow(
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
        CHECK_ASSIGN_RETURN_IF_FAILED(result, outResult);
    }

    ShowWindow(hwnd, properties.showMode);

    return hwnd;
}


HRESULT fastdx::runMainLoop(std::function<void(double)> updateFunction, std::function<void()> drawFunction) {
    MSG msg = {};
    const double kDesiredUpdateTimeMs = 1.0 / 60.0;
    double remainingElapsedTimeMs = 0.0;

    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        static high_resolution_clock::time_point lastClockTime = high_resolution_clock::now();
        high_resolution_clock::time_point currentClockTime = high_resolution_clock::now();

        double elapsedTimeMs = duration<double, std::milli>(currentClockTime - lastClockTime).count();
        elapsedTimeMs += remainingElapsedTimeMs;
        lastClockTime = currentClockTime;

        if (updateFunction) {
            int updateCycles = (int)(elapsedTimeMs / kDesiredUpdateTimeMs);
            remainingElapsedTimeMs = max(0.0, elapsedTimeMs - updateCycles * kDesiredUpdateTimeMs);

            for (int32_t i = 0; i < updateCycles; ++i) {
                updateFunction(kDesiredUpdateTimeMs);
            }
        }

        if (drawFunction) {
            drawFunction();
        }
    }

    return S_OK;
}