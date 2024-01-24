#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <dxgi1_6.h>
#include <d3d12.h>

#include <functional>
#include <memory>
#include <stdint.h>
#include <vector>

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if (p) { (p)->Release(); (p)=nullptr; } }
#endif


namespace fastdx {

    struct PtrDeleter {
        void operator() (IUnknown* ptr) {
            SAFE_RELEASE(ptr);
        }
    };

    typedef std::shared_ptr<ID3D12CommandAllocator> ID3D12CommandAllocatorPtr;
    typedef std::shared_ptr<ID3D12CommandQueue> ID3D12CommandQueuePtr;
    typedef std::shared_ptr<ID3D12DescriptorHeap> ID3D12DescriptorHeapPtr;
    typedef std::shared_ptr<ID3D12Device2> ID3D12DevicePtr;
    typedef std::shared_ptr<ID3D12Fence> ID3D12FencePtr;
    typedef std::shared_ptr <ID3D12GraphicsCommandList6> ID3D12GraphicsCommandListPtr;
    typedef std::shared_ptr<IDXGISwapChain3> IDXGISwapChainPtr;

    class D3D12DeviceWrapper;
    typedef std::shared_ptr<D3D12DeviceWrapper> D3D12DeviceWrapperPtr;

    struct WindowProperties {
        int32_t width = 1280;
        int32_t height = 720;
        int32_t showMode = SW_SHOW;
        WCHAR title[64] = L"fastdx";
        bool isFullScreen = false;
    };

    HRESULT createWindow(const WindowProperties& properties, HWND* optOutWindow = nullptr);
    HRESULT runMainLoop(std::function<void(double)> updateFunction = nullptr, std::function<void()> drawFunction = nullptr);
    D3D12DeviceWrapperPtr createDevice(D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_12_2, HRESULT* outResult = nullptr);


    class D3D12DeviceWrapper {
    public:
        D3D12DeviceWrapper(ID3D12DevicePtr device) :
            _device(device) {}

        ID3D12DevicePtr d3dDevice() const { return _device; }

        ID3D12CommandAllocatorPtr createCommandAllocator(D3D12_COMMAND_LIST_TYPE commandType,
            HRESULT* outResult = nullptr);

        ID3D12GraphicsCommandListPtr createCommandList(uint32_t nodeMask, D3D12_COMMAND_LIST_TYPE commandType,
            ID3D12CommandAllocatorPtr allocator, HRESULT* outResult = nullptr);

        ID3D12CommandQueuePtr createCommandQueue(D3D12_COMMAND_LIST_TYPE type, HRESULT* outResult = nullptr);

        ID3D12DescriptorHeapPtr createHeapDescriptor(int32_t count, D3D12_DESCRIPTOR_HEAP_TYPE heapType,
            HRESULT* outResult = nullptr);

        IDXGISwapChainPtr createWindowSwapChain(ID3D12CommandQueuePtr commandQueue, uint32_t bufferCount, DXGI_FORMAT format,
            HRESULT* outResult = nullptr);
        IDXGISwapChainPtr createWindowSwapChain(ID3D12CommandQueuePtr commandQueue, DXGI_SWAP_CHAIN_DESC1 swapChainDesc,
            HWND hwnd, HRESULT* outResult = nullptr);

        void createRenderTargetViews(IDXGISwapChainPtr swapChain, ID3D12DescriptorHeapPtr heap,
            HRESULT* outResult = nullptr);

    private:
        ID3D12DevicePtr _device;
    };
};


/// Defaults
namespace fastdx {

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
    D3D12_RASTERIZER_DESC defaultRasterDesc() { return DEFAULT_D3D12_RASTERIZER_DESC(); }


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
    D3D12_BLEND_DESC defaultBlendDesc() { return DEFAULT_D3D12_BLEND_DESC(); }
};