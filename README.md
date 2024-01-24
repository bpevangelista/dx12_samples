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

    // Triple frame-buffer chain
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = fastdx::defaultSwapChainDesc(hwnd);
    swapChainDesc.BufferCount = kFrameCount;
    swapChainDesc.Format = kFrameFormat;
    swapChain = device->createSwapChainForHwnd(commandQueue, swapChainDesc, hwnd);

    // Create a heap of descriptors, then fill them with swap chain render targets desc
    swapChainRtvHeap = device->createHeapDescriptor(8, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    renderTargets = device->createRenderTargetViews(swapChain, swapChainRtvHeap);

    // Create one command allocator per frame buffer
    for (int32_t i = 0; i < kFrameCount; ++i) {
        commandAllocators[i] = device->createCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT);
    }
    commandList = device->createCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocators[0]);

    // Fence to wait for available completed frames
    swapFence = device->createFence(swapFenceCounter++, D3D12_FENCE_FLAG_NONE);
    fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
}
```

### Pipeline Setup
```cpp
    // Read VS and PS
    readShader(L"simple_vs.cso", vertexShader);
    readShader(L"simple_ps.cso", pixelShader);
    
    // Create a root signature for shaders
    pipelineRootSignature = device->createRootSignature(0, vertexShader.data(), vertexShader.size());

    // Create a pipeline
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc = fastdx::defaultGraphicsPipelineDesc(kFrameFormat);
    pipelineDesc.pRootSignature = pipelineRootSignature.get();
    pipelineDesc.VS = { vertexShader.data(), vertexShader.size() };
    pipelineDesc.PS = { pixelShader.data(), pixelShader.size() };
    pipelineState = device->createGraphicsPipelineState(pipelineDesc);
```
