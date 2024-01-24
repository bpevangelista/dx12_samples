# DX12 Samples

### Window handling and Main loop:
```cpp
void update(double elapsedTimeMs) {}
void draw() {}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    fastdx::WindowProperties prop;
    HWND hwnd = fastdx::createWindow(prop);
    return fastdx::runMainLoop(update, draw);
}
```

### Small Initialization Blob
```cpp
void initializeD3d(HWND hwnd) {
    // Create a device and queue to dispatch command lists
    device = fastdx::createDevice(D3D_FEATURE_LEVEL_12_2);
    commandQueue = device->createCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);

    // Create triple frame-buffer chain
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = fastdx::defaultSwapChainDesc(hwnd);
    swapChainDesc.BufferCount = 3;
    swapChainDesc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
    swapChain = device->createSwapChainForHwnd(commandQueue, swapChainDesc, hwnd);

    // Create descriptors heap, then fill it with the swap chain render targets desc
    swapChainRtvHeap = device->createHeapDescriptor(3, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    renderTargets = device->createRenderTargetViews(swapChain, swapChainRtvHeap);

    // Create one command allocator per frame
    for (int32_t i = 0; i < kFrameCount; ++i) {
        commandAllocators[i] = device->createCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT);
    }
    // Single command list reused across command allocators
    commandList = device->createCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, 
        commandAllocators[0]);

    // Create fence to wait for available completed frames
    swapFence = device->createFence(swapFenceCounter++, D3D12_FENCE_FLAG_NONE);
    fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
}
```

### Pipeline Setup
```cpp
// Read VS and PS
readShader(L"simple_vs.cso", vertexShader);
readShader(L"simple_ps.cso", pixelShader);
    
// Create root signature for VS/PS
pipelineRootSignature = device->createRootSignature(0, vertexShader.data(), vertexShader.size());

// Create pipeline
D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc = fastdx::defaultGraphicsPipelineDesc(kFrameFormat);
pipelineDesc.pRootSignature = pipelineRootSignature.get();
pipelineDesc.VS = { vertexShader.data(), vertexShader.size() };
pipelineDesc.PS = { pixelShader.data(), pixelShader.size() };
pipelineState = device->createGraphicsPipelineState(pipelineDesc);
```
