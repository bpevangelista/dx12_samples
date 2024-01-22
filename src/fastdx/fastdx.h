#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <dxgi1_6.h>
#include <d3d12.h>
//#include <DirectXMath.h>
#include <wrl.h> // Windows Runtime Library

#include <cmath>
#include <map>
#include <memory>
#include <stdint.h>
#include <string>
#include <vector>


namespace fastdx {

    typedef std::shared_ptr<ID3D12Device2> ID3D12Device2Ptr;
    typedef std::shared_ptr<ID3D12CommandQueue> ID3D12CommandQueuePtr;
    typedef std::shared_ptr <ID3D12GraphicsCommandList6> ID3D12GraphicsCommandList6Ptr;
    typedef std::shared_ptr<IDXGISwapChain3> IDXGISwapChain3Ptr;
    typedef std::shared_ptr<ID3D12DescriptorHeap> ID3D12DescriptorHeapPtr;

    struct WindowProperties {
        int32_t width = 1280;
        int32_t height = 720;
        int32_t showMode = SW_SHOW;
        WCHAR title[64] = L"fastdx";
        bool isFullScreen = false;
    };

    HRESULT createWindow(const WindowProperties& properties, HWND* optOutWindow = nullptr);
    HRESULT runMainLoop();

    ID3D12Device2Ptr createDevice(D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_12_2, HRESULT* outResult = nullptr);
    ID3D12CommandQueuePtr createCommandQueue(ID3D12Device2Ptr device, D3D12_COMMAND_LIST_TYPE type, HRESULT* outResult = nullptr);
    IDXGISwapChain3Ptr createWindowSwapChain(ID3D12Device2Ptr device, ID3D12CommandQueuePtr commandQueue, DXGI_FORMAT format, HRESULT* outResult = nullptr);

    ID3D12DescriptorHeapPtr createHeapDescriptor(ID3D12Device2Ptr device, int32_t count, D3D12_DESCRIPTOR_HEAP_TYPE heapType, HRESULT* outResult = nullptr);
    void createRenderTargetViews(ID3D12Device2Ptr device, IDXGISwapChain3Ptr swapChain, ID3D12DescriptorHeapPtr heap, HRESULT* outResult = nullptr);
};

