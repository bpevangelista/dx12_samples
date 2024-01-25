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
    typedef std::shared_ptr<ID3D12GraphicsCommandList6> ID3D12GraphicsCommandListPtr;
    typedef std::shared_ptr<ID3D12PipelineState> ID3D12PipelineStatePtr;
    typedef std::shared_ptr<ID3D12Resource> ID3D12ResourcePtr;
    typedef std::shared_ptr<ID3D12RootSignature> ID3D12RootSignaturePtr;

    typedef std::shared_ptr<ID3DBlob> ID3DBlobPtr;
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

    HWND createWindow(const WindowProperties& properties, HRESULT* outResult = nullptr);
    int runMainLoop(std::function<void(double)> updateFunction = nullptr, std::function<void()> drawFunction = nullptr);
    extern std::function<void()> onWindowDestroy;

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

        ID3D12FencePtr createFence(uint64_t initialValue, D3D12_FENCE_FLAGS flags, HRESULT* outResult = nullptr);

        ID3D12PipelineStatePtr createGraphicsPipelineState(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc,
            HRESULT* outResult = nullptr);

        ID3D12DescriptorHeapPtr createHeapDescriptor(int32_t count, D3D12_DESCRIPTOR_HEAP_TYPE heapType,
            HRESULT* outResult = nullptr);

        std::vector<ID3D12ResourcePtr> createRenderTargetViews(IDXGISwapChainPtr swapChain, ID3D12DescriptorHeapPtr heap,
            HRESULT* outResult = nullptr);

        ID3D12RootSignaturePtr createRootSignature(uint32_t nodeMask, const void* data, size_t dataSizeInBytes,
            HRESULT* outResult = nullptr);

        IDXGISwapChainPtr createSwapChainForHwnd(ID3D12CommandQueuePtr commandQueue, DXGI_SWAP_CHAIN_DESC1 swapChainDesc,
            HWND hwnd, HRESULT* outResult = nullptr);

    private:
        ID3D12DevicePtr _device;
    };
};


/// Defaults
namespace fastdx {

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
    inline DEFAULT_D3D12_DEPTH_STENCIL_DESC defaultDepthStencilDesc() { return DEFAULT_D3D12_DEPTH_STENCIL_DESC(); }


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


    struct DEFAULT_DXGI_SWAP_CHAIN_DESC1 : public DXGI_SWAP_CHAIN_DESC1 {
        DEFAULT_DXGI_SWAP_CHAIN_DESC1(const HWND hwnd) {
            RECT windowRect;
            GetWindowRect(hwnd, &windowRect);
            Width = windowRect.right - windowRect.left;
            Height = windowRect.bottom - windowRect.top;
            Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            Stereo = FALSE;
            SampleDesc = DXGI_SAMPLE_DESC{ 1, 0 };
            BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            BufferCount = 2;
            Scaling = DXGI_SCALING_STRETCH;
            SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
            AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
            Flags = 0;
        }
    };
    inline DXGI_SWAP_CHAIN_DESC1 defaultSwapChainDesc(const HWND hwnd) { return DEFAULT_DXGI_SWAP_CHAIN_DESC1(hwnd); }


    struct DEFAULT_D3D12_GRAPHICS_PIPELINE_STATE_DESC :
        public D3D12_GRAPHICS_PIPELINE_STATE_DESC {
        DEFAULT_D3D12_GRAPHICS_PIPELINE_STATE_DESC(
            DXGI_FORMAT renderTargetFormat) {
            memset(this, 0, sizeof(*this));
            BlendState = fastdx::defaultBlendDesc();
            SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
            RasterizerState = fastdx::defaultRasterizerDesc();
            DepthStencilState = fastdx::defaultDepthStencilDesc();
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
};
