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

    typedef std::shared_ptr<ID3D12Device2> ID3D12DevicePtr;
    typedef std::shared_ptr<ID3D12CommandAllocator> ID3D12CommandAllocatorPtr;
    typedef std::shared_ptr <ID3D12GraphicsCommandList6> ID3D12GraphicsCommandListPtr;
    typedef std::shared_ptr<ID3D12DescriptorHeap> ID3D12DescriptorHeapPtr;
    typedef std::shared_ptr<ID3D12CommandQueue> ID3D12CommandQueuePtr;
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

        ID3D12CommandQueuePtr createCommandQueue(D3D12_COMMAND_LIST_TYPE type, HRESULT* outResult = nullptr);

        IDXGISwapChainPtr createWindowSwapChain(ID3D12CommandQueuePtr commandQueue, uint32_t bufferCount, DXGI_FORMAT format,
            HRESULT* outResult = nullptr);
        IDXGISwapChainPtr createWindowSwapChain(ID3D12CommandQueuePtr commandQueue,DXGI_SWAP_CHAIN_DESC1 swapChainDesc,
            HWND hwnd, HRESULT* outResult = nullptr);

        ID3D12DescriptorHeapPtr createHeapDescriptor(int32_t count, D3D12_DESCRIPTOR_HEAP_TYPE heapType,
            HRESULT* outResult = nullptr);

        ID3D12CommandAllocatorPtr createCommandAllocator(D3D12_COMMAND_LIST_TYPE commandType,
            HRESULT* outResult = nullptr);

        ID3D12GraphicsCommandListPtr createCommandList(uint32_t nodeMask, D3D12_COMMAND_LIST_TYPE commandType,
            ID3D12CommandAllocatorPtr allocator, HRESULT* outResult = nullptr);

        void createRenderTargetViews(IDXGISwapChainPtr swapChain, ID3D12DescriptorHeapPtr heap,
            HRESULT* outResult = nullptr);

    private:
        ID3D12DevicePtr _device;
    };
};

