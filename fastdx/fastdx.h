#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <chrono>
#include <functional>
#include <memory>
#include <stdint.h>
#include <vector>


///
/// fastdx Header - D3D12 Lightweight Wrapper for Quick Prototyping
///
namespace fastdx {
    class D3D12DeviceWrapper;
    typedef std::shared_ptr<D3D12DeviceWrapper> D3D12DeviceWrapperPtr;

    typedef std::shared_ptr<ID3D12CommandAllocator> ID3D12CommandAllocatorPtr;
    typedef std::shared_ptr<ID3D12CommandQueue> ID3D12CommandQueuePtr;
    typedef std::shared_ptr<ID3D12DescriptorHeap> ID3D12DescriptorHeapPtr;
    typedef std::shared_ptr<ID3D12Device2> ID3D12DevicePtr;
    typedef std::shared_ptr<ID3D12Fence> ID3D12FencePtr;
    typedef std::shared_ptr<ID3D12GraphicsCommandList6> ID3D12GraphicsCommandListPtr;
    typedef std::shared_ptr<ID3D12PipelineState> ID3D12PipelineStatePtr;
    typedef std::shared_ptr<ID3D12Resource> ID3D12ResourcePtr;
    typedef std::shared_ptr<ID3D12RootSignature> ID3D12RootSignaturePtr;
    typedef std::shared_ptr<ID3DBlob> ID3DBlobPtr;
    typedef std::shared_ptr<IDXGISwapChain3> IDXGISwapChainPtr;

    struct WindowProperties {
        int32_t width = 1280;
        int32_t height = 720;
        int32_t showMode = SW_SHOW;
        WCHAR title[64] = L"fastdx";
        bool isFullScreen = false;
    };
    inline std::function<void()> onWindowDestroy = nullptr;


    ///
    /// Window helpers
    ///
    HWND createWindow(const WindowProperties& properties, HRESULT* outResult = nullptr);
    int runMainLoop(std::function<void(double)> updateFunction = nullptr,
        std::function<void()> drawFunction = nullptr);


    ///
    /// Device Wrapper
    ///
    D3D12DeviceWrapperPtr createDevice(D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_12_2,
        HRESULT* outResult = nullptr);

    class D3D12DeviceWrapper {
    public:
        D3D12DeviceWrapper(ID3D12DevicePtr device) : _device(device) {}

        inline ID3D12DevicePtr d3dDevice() const { return _device; }

        ID3D12CommandAllocatorPtr createCommandAllocator(D3D12_COMMAND_LIST_TYPE commandType,
            HRESULT* outResult = nullptr);

        ID3D12GraphicsCommandListPtr createCommandList(uint32_t nodeMask, D3D12_COMMAND_LIST_TYPE commandType,
            ID3D12CommandAllocatorPtr allocator, HRESULT* outResult = nullptr);

        ID3D12CommandQueuePtr createCommandQueue(D3D12_COMMAND_LIST_TYPE type, HRESULT* outResult = nullptr);

        ID3D12ResourcePtr createCommittedResource(const D3D12_HEAP_PROPERTIES& heapProperties,
            D3D12_HEAP_FLAGS heapFlags, const D3D12_RESOURCE_DESC& desc, D3D12_RESOURCE_STATES initialState,
            const D3D12_CLEAR_VALUE* optOptimalClearValue, HRESULT* outResult = nullptr);

        ID3D12FencePtr createFence(uint64_t initialValue, D3D12_FENCE_FLAGS flags, HRESULT* outResult = nullptr);

        ID3D12PipelineStatePtr createGraphicsPipelineState(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc,
            HRESULT* outResult = nullptr);

        ID3D12DescriptorHeapPtr createDescriptorHeap(int32_t count, D3D12_DESCRIPTOR_HEAP_TYPE heapType,
            HRESULT* outResult = nullptr);

        std::vector<ID3D12ResourcePtr> createRenderTargetViews(IDXGISwapChainPtr swapChain,
            ID3D12DescriptorHeapPtr heap, HRESULT* outResult = nullptr);

        ID3D12RootSignaturePtr createRootSignature(uint32_t nodeMask, const void* data, size_t dataSizeInBytes,
            HRESULT* outResult = nullptr);

        IDXGISwapChainPtr createSwapChainForHwnd(ID3D12CommandQueuePtr commandQueue,
            const DXGI_SWAP_CHAIN_DESC1& swapChainDesc, HWND hwnd, HRESULT* outResult = nullptr);

        void createConstantBufferView(const D3D12_CONSTANT_BUFFER_VIEW_DESC& desc,
            D3D12_CPU_DESCRIPTOR_HANDLE handle);

        void createDepthStencilView(ID3D12ResourcePtr resource, const D3D12_DEPTH_STENCIL_VIEW_DESC& desc,
            D3D12_CPU_DESCRIPTOR_HANDLE handle);

        void createRenderTargetView(ID3D12ResourcePtr resource, const D3D12_RENDER_TARGET_VIEW_DESC& desc,
            D3D12_CPU_DESCRIPTOR_HANDLE handle);

        void createShaderResourceView(ID3D12ResourcePtr resource, const D3D12_SHADER_RESOURCE_VIEW_DESC& desc,
            D3D12_CPU_DESCRIPTOR_HANDLE handle);

        uint32_t getDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE descriptorHeapType);

    private:
        ID3D12DevicePtr _device;
    };
}

///
/// fastdxu Header - D3D12 Utilities
///
namespace fastdxu {
    D3D12_BLEND_DESC defaultBlendDesc();

    D3D12_DEPTH_STENCIL_DESC defaultDepthStencilDesc();

    D3D12_INDEX_BUFFER_VIEW indexBufferView(D3D12_GPU_VIRTUAL_ADDRESS BufferLocation, UINT SizeInBytes,
        DXGI_FORMAT Format = DXGI_FORMAT_R16_UINT);

    D3D12_RASTERIZER_DESC defaultRasterizerDesc();

    D3D12_RESOURCE_BARRIER resourceBarrierTransition(fastdx::ID3D12ResourcePtr resource, D3D12_RESOURCE_STATES beforeState, D3D12_RESOURCE_STATES afterState);

    D3D12_RESOURCE_DESC resourceBufferDesc(uint32_t width,
        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);

    D3D12_RESOURCE_DESC resourceTexDesc(D3D12_RESOURCE_DIMENSION dimension, uint32_t width,
        uint32_t height, uint16_t depth, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags);

    D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc(D3D12_SRV_DIMENSION dimension);

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc(const HWND hwnd, uint32_t bufferCount = 2,
        DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM);

    D3D12_GRAPHICS_PIPELINE_STATE_DESC defaultGraphicsPipelineDesc(DXGI_FORMAT renderTargetFormat);
};


///
/// Implementation
///
#if defined(FASTDX_IMPLEMENTATION)

#ifndef SAFE_FREE
#define SAFE_FREE(p) { if (p) { free(p); (p)=nullptr; } }
#endif

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) { if (p) { (p)->Release(); (p)=nullptr; } }
#endif

#ifndef CHECK_ASSIGN_RETURN_IF_FAILED_
#define CHECK_ASSIGN_RETURN_IF_FAILED_(HRESULTVAR, OUTVAR, ...) { if (_checkFailedAndAssign(HRESULTVAR, OUTVAR)) { return __VA_ARGS__; } }
#endif

#ifndef CHECK_ASSIGN_RETURN_IF_FAILED
#define CHECK_ASSIGN_RETURN_IF_FAILED(HRESULTVAR, OUTVAR) { if (_checkFailedAndAssign(HRESULTVAR, OUTVAR)) { return nullptr; } }
#endif

///
/// Common
///
namespace fastdx {
    struct PtrDeleter {
        void operator() (IUnknown* ptr) {
            SAFE_RELEASE(ptr);
        }
    };

    inline bool _checkFailedAndAssign(HRESULT hr, HRESULT* outResult = nullptr) {
        if (outResult != nullptr) {
            *outResult = hr;
        }
        return FAILED(hr);
    }
};


///
/// Window Implementation
///
namespace fastdx {
    using namespace std::chrono;

    LRESULT CALLBACK _WindowProcKeyDown(HWND hWnd, WPARAM wParam, LPARAM lParam) {
        switch (wParam) {
        case VK_ESCAPE:
            SendMessage(hWnd, WM_CLOSE, 0, 0);
            break;
        }

        return S_OK;
    }


    LRESULT CALLBACK _WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
        case WM_PAINT:
            break;

        case WM_SIZE:
            break;

        case WM_KEYDOWN:
            _WindowProcKeyDown(hWnd, wParam, lParam);
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        }

        return DefWindowProc(hWnd, msg, wParam, lParam);
    }


    HWND createWindow(const WindowProperties& properties, HRESULT* outResult) {
        HINSTANCE hInstance = (HINSTANCE)GetModuleHandle(nullptr);

        WNDCLASSEX wc = { 0 };
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.style = properties.isFullScreen ? 0 : CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = (WNDPROC)_WindowProc;
        wc.hInstance = hInstance;
        wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = L"fastdx";
        RegisterClassEx(&wc);

        HWND hwnd = CreateWindow(
            wc.lpszClassName,
            properties.title,
            properties.isFullScreen ? WS_POPUP : WS_OVERLAPPEDWINDOW,
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


    int runMainLoop(std::function<void(double)> updateFunction, std::function<void()> drawFunction) {
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

        fastdx::onWindowDestroy();

        return static_cast<int>(msg.wParam);
    }
};


///
/// D3D12DeviceWrapper Implementation
///
namespace fastdx {
    std::shared_ptr<IDXGIFactory4> _getOrCreateDXIG(HRESULT* outResult) {
        static std::shared_ptr<IDXGIFactory4> _dxgiFactory = nullptr;
        if (_dxgiFactory) {
            if (outResult) {
                *outResult = S_OK;
            }
            return _dxgiFactory;
        }

        uint32_t dxgiFactoryFlags = 0;

#if defined(_DEBUG)
        dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;

        ID3D12Debug1* debugController = nullptr;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
            debugController->EnableDebugLayer();
            debugController->SetEnableGPUBasedValidation(true);
            debugController->SetEnableSynchronizedCommandQueueValidation(true);
        }
        SAFE_RELEASE(debugController);

        IDXGIInfoQueue* dxgiInfoQueue = nullptr;
        if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiInfoQueue)))) {
            dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
            dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);
        }
        SAFE_RELEASE(dxgiInfoQueue);
#endif

        HRESULT hr;
        IDXGIFactory4* dxgiFactory = nullptr;
        hr = CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&dxgiFactory));
        if (SUCCEEDED(hr)) {
            _dxgiFactory = std::shared_ptr<IDXGIFactory4>(dxgiFactory, PtrDeleter());
        }

        CHECK_ASSIGN_RETURN_IF_FAILED(hr, outResult);
        return _dxgiFactory;
    }


    D3D12DeviceWrapperPtr createDevice(D3D_FEATURE_LEVEL featureLevel, HRESULT* outResult) {
        HRESULT hr = E_FAIL;
        std::shared_ptr<IDXGIFactory4> dxgiFactory = _getOrCreateDXIG(&hr);

        ID3D12Device2* device = nullptr;
        IDXGIAdapter1* hardwareAdapter = nullptr;
        std::vector <IDXGIAdapter1*> hardwareAdapters;

        for (int32_t i = 0; dxgiFactory->EnumAdapters1(i, &hardwareAdapter) != DXGI_ERROR_NOT_FOUND; ++i) {
            hardwareAdapters.push_back(hardwareAdapter);

            DXGI_ADAPTER_DESC1 desc;
            hardwareAdapter->GetDesc1(&desc);
            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
                continue;
            }

            hr = D3D12CreateDevice(hardwareAdapter, featureLevel, IID_PPV_ARGS(&device));
            if (SUCCEEDED(hr)) {
                break;
            }
        }

        for (IDXGIAdapter1* adapter : hardwareAdapters) {
            SAFE_RELEASE(adapter);
        }

        CHECK_ASSIGN_RETURN_IF_FAILED(hr, outResult);

        auto devicePtr = std::shared_ptr<ID3D12Device2>(device, PtrDeleter());
        return D3D12DeviceWrapperPtr(new D3D12DeviceWrapper(devicePtr));
    }


    ID3D12CommandAllocatorPtr D3D12DeviceWrapper::createCommandAllocator(D3D12_COMMAND_LIST_TYPE commandType, HRESULT* outResult) {

        HRESULT hr;
        ID3D12CommandAllocator* commandAllocator = nullptr;
        hr = _device->CreateCommandAllocator(commandType, IID_PPV_ARGS(&commandAllocator));

        CHECK_ASSIGN_RETURN_IF_FAILED(hr, outResult);
        return ID3D12CommandAllocatorPtr(commandAllocator, PtrDeleter());
    }


    ID3D12GraphicsCommandListPtr D3D12DeviceWrapper::createCommandList(uint32_t nodeMask, D3D12_COMMAND_LIST_TYPE commandType,
        ID3D12CommandAllocatorPtr allocator, HRESULT* outResult) {

        HRESULT hr;
        ID3D12GraphicsCommandList6* commandList = nullptr;
        hr = _device->CreateCommandList(nodeMask, commandType, allocator.get(), nullptr, IID_PPV_ARGS(&commandList));

        CHECK_ASSIGN_RETURN_IF_FAILED(hr, outResult);
        return ID3D12GraphicsCommandListPtr(commandList, PtrDeleter());
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


    ID3D12ResourcePtr D3D12DeviceWrapper::createCommittedResource(const D3D12_HEAP_PROPERTIES& heapProperties,
        D3D12_HEAP_FLAGS heapFlags, const D3D12_RESOURCE_DESC& desc, D3D12_RESOURCE_STATES initialState,
        const D3D12_CLEAR_VALUE* optOptimalClearValue, HRESULT* outResult) {

        HRESULT hr;
        ID3D12Resource* resource;
        hr = _device->CreateCommittedResource(&heapProperties, heapFlags, &desc, initialState, optOptimalClearValue,
            IID_PPV_ARGS(&resource));

        CHECK_ASSIGN_RETURN_IF_FAILED(hr, outResult);
        return ID3D12ResourcePtr(resource, PtrDeleter());
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


    ID3D12DescriptorHeapPtr D3D12DeviceWrapper::createDescriptorHeap(int32_t count, D3D12_DESCRIPTOR_HEAP_TYPE heapType,
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
        ID3D12DescriptorHeap* heapDescriptor = nullptr;
        hr = _device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&heapDescriptor));

        CHECK_ASSIGN_RETURN_IF_FAILED(hr, outResult);
        return ID3D12DescriptorHeapPtr(heapDescriptor, PtrDeleter());
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
        for (uint32_t i = 0; i < swapChainDesc.BufferCount; ++i) {
            ID3D12Resource* renderTarget = nullptr;
            hr = swapChain->GetBuffer(i, IID_PPV_ARGS(&renderTarget));
            CHECK_ASSIGN_RETURN_IF_FAILED_(hr, outResult, std::vector<ID3D12ResourcePtr>());
            resources.push_back(ID3D12ResourcePtr(renderTarget, PtrDeleter()));

            _device->CreateRenderTargetView(renderTarget, nullptr, heapHandle);
            heapHandle.ptr += heapDescriptorSize;
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
        const DXGI_SWAP_CHAIN_DESC1& swapChainDesc, HWND hwnd, HRESULT* outResult) {

        HRESULT hr;
        std::shared_ptr<IDXGIFactory4> dxgiFactory = _getOrCreateDXIG(&hr);
        CHECK_ASSIGN_RETURN_IF_FAILED(hr, outResult);

        IDXGISwapChain1* swapChain1 = nullptr;
        hr = dxgiFactory->CreateSwapChainForHwnd(
            commandQueue.get(),     // Link to Command Queue
            hwnd,                   // Link to Window
            &swapChainDesc,
            nullptr,
            nullptr,
            &swapChain1
        );
        CHECK_ASSIGN_RETURN_IF_FAILED(hr, outResult);

        IDXGISwapChain3* swapChain3 = nullptr;
        hr = swapChain1->QueryInterface(__uuidof(IDXGISwapChain3), reinterpret_cast<void**>(&swapChain3));
        SAFE_RELEASE(swapChain1);
        CHECK_ASSIGN_RETURN_IF_FAILED(hr, outResult);

        hr = dxgiFactory->MakeWindowAssociation(hwnd, 0);
        CHECK_ASSIGN_RETURN_IF_FAILED(hr, outResult);

        return IDXGISwapChainPtr(swapChain3, PtrDeleter());
    }


    void D3D12DeviceWrapper::createConstantBufferView(const D3D12_CONSTANT_BUFFER_VIEW_DESC& desc,
        D3D12_CPU_DESCRIPTOR_HANDLE handle) {
        _device->CreateConstantBufferView(&desc, handle);
    }


    void D3D12DeviceWrapper::createDepthStencilView(ID3D12ResourcePtr resource, const D3D12_DEPTH_STENCIL_VIEW_DESC& desc,
        D3D12_CPU_DESCRIPTOR_HANDLE handle) {
        _device->CreateDepthStencilView(resource.get(), &desc, handle);
    }


    void D3D12DeviceWrapper::createRenderTargetView(ID3D12ResourcePtr resource, const D3D12_RENDER_TARGET_VIEW_DESC& desc,
        D3D12_CPU_DESCRIPTOR_HANDLE handle) {
        _device->CreateRenderTargetView(resource.get(), &desc, handle);
    }


    void D3D12DeviceWrapper::createShaderResourceView(ID3D12ResourcePtr resource, const D3D12_SHADER_RESOURCE_VIEW_DESC& desc,
        D3D12_CPU_DESCRIPTOR_HANDLE handle) {
        _device->CreateShaderResourceView(resource.get(), &desc, handle);
    }

    inline uint32_t D3D12DeviceWrapper::getDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE descriptorHeapType) {
        return _device->GetDescriptorHandleIncrementSize(descriptorHeapType);
    }

};
#endif // FASTDX_IMPLEMENTATION


///
/// INLINED D3D12 Utilities
///
namespace fastdxu {

    struct DEFAULT_D3D12_BLEND_DESC : public D3D12_BLEND_DESC {
        DEFAULT_D3D12_BLEND_DESC() {
            AlphaToCoverageEnable = FALSE;
            IndependentBlendEnable = FALSE;

            for (int32_t i = 0; i < _countof(RenderTarget); ++i) {
                RenderTarget[i].BlendEnable = FALSE;
                RenderTarget[i].LogicOpEnable = FALSE;
                RenderTarget[i].SrcBlend = D3D12_BLEND_ONE;
                RenderTarget[i].DestBlend = D3D12_BLEND_ZERO;
                RenderTarget[i].BlendOp = D3D12_BLEND_OP_ADD;
                RenderTarget[i].SrcBlendAlpha = D3D12_BLEND_ONE;
                RenderTarget[i].DestBlendAlpha = D3D12_BLEND_ZERO;
                RenderTarget[i].BlendOpAlpha = D3D12_BLEND_OP_ADD;
                RenderTarget[i].LogicOp = D3D12_LOGIC_OP_NOOP;
                RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
            }
        }
    };
    inline D3D12_BLEND_DESC defaultBlendDesc() { return DEFAULT_D3D12_BLEND_DESC(); }


    struct DEFAULT_D3D12_DEPTH_STENCIL_DESC : public D3D12_DEPTH_STENCIL_DESC {
        DEFAULT_D3D12_DEPTH_STENCIL_DESC() {
            DepthEnable = TRUE;
            DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
            DepthFunc = D3D12_COMPARISON_FUNC_LESS;
            StencilEnable = FALSE;
            StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
            StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
            FrontFace = { D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP,
                D3D12_COMPARISON_FUNC_ALWAYS };
            BackFace = { D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP,
                D3D12_COMPARISON_FUNC_ALWAYS };
        }
    };
    inline D3D12_DEPTH_STENCIL_DESC defaultDepthStencilDesc() { return DEFAULT_D3D12_DEPTH_STENCIL_DESC(); }


    struct DEFAULT_D3D12_RASTERIZER_DESC : public D3D12_RASTERIZER_DESC {
        DEFAULT_D3D12_RASTERIZER_DESC() {
            FillMode = D3D12_FILL_MODE_SOLID;
            CullMode = D3D12_CULL_MODE_BACK;
            FrontCounterClockwise = FALSE;
            DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
            DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
            SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
            DepthClipEnable = TRUE;
            MultisampleEnable = FALSE;
            AntialiasedLineEnable = FALSE;
            ForcedSampleCount = 0;
            ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
        }
    };
    inline D3D12_RASTERIZER_DESC defaultRasterizerDesc() { return DEFAULT_D3D12_RASTERIZER_DESC(); }


    struct DEFAULT_D3D12_GRAPHICS_PIPELINE_STATE_DESC :
        public D3D12_GRAPHICS_PIPELINE_STATE_DESC {
        DEFAULT_D3D12_GRAPHICS_PIPELINE_STATE_DESC(
            DXGI_FORMAT renderTargetFormat) {
            memset(this, 0, sizeof(*this));
            BlendState = fastdxu::defaultBlendDesc();
            SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
            RasterizerState = fastdxu::defaultRasterizerDesc();
            DepthStencilState = fastdxu::defaultDepthStencilDesc();
            PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            NumRenderTargets = 1;
            RTVFormats[0] = renderTargetFormat;
            DSVFormat = DXGI_FORMAT_D32_FLOAT;
            SampleDesc = DXGI_SAMPLE_DESC{ 1, 0 };
            Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
        }
    };
    inline D3D12_GRAPHICS_PIPELINE_STATE_DESC defaultGraphicsPipelineDesc(DXGI_FORMAT renderTargetFormat) {
        return DEFAULT_D3D12_GRAPHICS_PIPELINE_STATE_DESC(renderTargetFormat);
    }


    inline D3D12_INDEX_BUFFER_VIEW indexBufferView(
        D3D12_GPU_VIRTUAL_ADDRESS BufferLocation, UINT SizeInBytes, DXGI_FORMAT Format) {
        return D3D12_INDEX_BUFFER_VIEW{
            BufferLocation,
            SizeInBytes,
            Format
        };
    }


    inline D3D12_RESOURCE_BARRIER resourceBarrierTransition(fastdx::ID3D12ResourcePtr resource,
        D3D12_RESOURCE_STATES beforeState = D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATES afterState = D3D12_RESOURCE_STATE_COMMON) {

        return D3D12_RESOURCE_BARRIER{
            D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
            D3D12_RESOURCE_BARRIER_FLAG_NONE,
            // Transition
            resource.get(),
            D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
            beforeState,
            afterState
        };
    }

    inline D3D12_RESOURCE_DESC resourceBufferDesc(uint32_t width, D3D12_RESOURCE_FLAGS flags) {
        return D3D12_RESOURCE_DESC{
            D3D12_RESOURCE_DIMENSION_BUFFER,
            0,                                      // 64KB alignment
            static_cast<uint64_t>(width),
            1,                                      // Unchangeable
            1,                                      // Unchangeable
            1,                                      // Unchangeable
            DXGI_FORMAT_UNKNOWN,                    // Unchangeable
            {1, 0},                                 // Unchangeable
            D3D12_TEXTURE_LAYOUT_ROW_MAJOR,         // Unchangeable
            flags,
        };
    }


    inline D3D12_RESOURCE_DESC resourceTexDesc(D3D12_RESOURCE_DIMENSION dimension, uint32_t width,
        uint32_t height, uint16_t depth, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags) {
        return D3D12_RESOURCE_DESC{
            dimension,
            0,                                      // 4MB for MSAA, 64KB otherwise
            static_cast<uint64_t>(width),
            height,
            depth,
            0,                                      // Mips from 0 to N
            format,
            {1, 0},
            D3D12_TEXTURE_LAYOUT_UNKNOWN,
            flags,
        };
    }


    struct DEFAULT_D3D12_SHADER_RESOURCE_VIEW_DESC : public D3D12_SHADER_RESOURCE_VIEW_DESC {
        DEFAULT_D3D12_SHADER_RESOURCE_VIEW_DESC(D3D12_SRV_DIMENSION dimension, DXGI_FORMAT format) {
            memset(this, 0, sizeof(D3D12_SHADER_RESOURCE_VIEW_DESC));
            ViewDimension = dimension;
            Format = format;
            Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        }
    };
    inline D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc(D3D12_SRV_DIMENSION dimension, DXGI_FORMAT format) {
        return DEFAULT_D3D12_SHADER_RESOURCE_VIEW_DESC(dimension, format);
    }


    struct DEFAULT_DXGI_SWAP_CHAIN_DESC1 : public DXGI_SWAP_CHAIN_DESC1 {
        DEFAULT_DXGI_SWAP_CHAIN_DESC1(const HWND hwnd, uint32_t bufferCount, DXGI_FORMAT format) {
            RECT windowRect;
            GetWindowRect(hwnd, &windowRect);
            Width = windowRect.right - windowRect.left;
            Height = windowRect.bottom - windowRect.top;
            Format = format;
            Stereo = FALSE;
            SampleDesc = DXGI_SAMPLE_DESC{ 1, 0 };
            BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            BufferCount = bufferCount;
            Scaling = DXGI_SCALING_STRETCH;
            SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
            AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
            Flags = 0;
        }
    };
    inline DXGI_SWAP_CHAIN_DESC1 swapChainDesc(const HWND hwnd, uint32_t bufferCount, DXGI_FORMAT format) {
        return DEFAULT_DXGI_SWAP_CHAIN_DESC1(hwnd, bufferCount, format);
    }
};
