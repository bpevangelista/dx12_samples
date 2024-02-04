# DX12 Samples with FastDX

FastDX is a Header-Only D3D12 Light-Wrapper for Clean and Quick Prototyping.
- Clear Screen (100 lines), Draw Triangle (200 lines), glTF (wip), DX12+CUDA (wip).

#### Window Handling and Main Loop:
```cpp
void update(double elapsedTimeMs) {}
void draw() {}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    fastdx::WindowProperties prop;
    HWND hwnd = fastdx::createWindow(prop);
    fastdx::onWindowDestroy = []() { waitGpu(true); };

    return fastdx::runMainLoop(update, draw);
}
```

#### Initialization
```cpp
void initializeD3d(HWND hwnd) {
    // Create a device and queue to dispatch command lists
    device = fastdx::createDevice(D3D_FEATURE_LEVEL_12_2);
    commandQueue = device->createCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);

    // Create triple frame-buffer chain
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = fastdx::defaultSwapChainDesc(hwnd, 2, DXGI_FORMAT_R10G10B10A2_UNORM);
    swapChain = device->createSwapChainForHwnd(commandQueue, swapChainDesc, hwnd);

    // Create descriptors heap, then fill it with the swap chain render targets desc
    swapChainRtvHeap = device->createHeapDescriptor(3, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    renderTargets = device->createRenderTargetViews(swapChain, swapChainRtvHeap);

    // Create one command allocator per frame
    for (int32_t i = 0; i < swapChainDesc.BufferCount; ++i) {
        commandAllocators[i] = device->createCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT);
    }
    // Create a single command list, reused across command allocators
    commandList = device->createCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocators[0]);

    // Create a fence to wait for available completed frames
    swapFence = device->createFence(swapFenceCounter++, D3D12_FENCE_FLAG_NONE);
    fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
}
```

#### Load Assets
```cpp
void loadAssets() {
    // Read VS, PS and Create their root signature
    readShader(L"simple_vs.cso", vertexShader);
    readShader(L"simple_ps.cso", pixelShader);
    pipelineRootSignature = device->createRootSignature(0, vertexShader.data(), vertexShader.size());
    
    // Create pipeline
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc = fastdx::defaultGraphicsPipelineDesc(kFrameFormat);
    pipelineDesc.pRootSignature = pipelineRootSignature.get();
    pipelineDesc.VS = { vertexShader.data(), vertexShader.size() };
    pipelineDesc.PS = { pixelShader.data(), pixelShader.size() };
    pipelineState = device->createGraphicsPipelineState(pipelineDesc);
    
    // Triangle Mesh
    struct Vertex { float x, y, z, w; float r, g, b, a; };
    Vertex triangleVertices[] = {   { -0.8f, -0.8f, 0.0f, 1.0f,    1.0f, 0.0f, 0.0f, 1.0f, },
                                    {  0.0f,  0.8f, 0.0f, 1.0f,    0.0f, 1.0f, 0.0f, 1.0f, },
                                    {  0.8f, -0.8f, 0.0f, 1.0f,    0.0f, 0.0f, 1.0f, 1.0f, } };
    
    // Create resource on D3D12_HEAP_TYPE_UPLOAD (ideally, copy to D3D12_HEAP_DEFAULT)
    D3D12_RESOURCE_DESC vertexBufferDesc = fastdx::defaultResourceBufferDesc(sizeof(triangleVertices));
    D3D12_HEAP_PROPERTIES defaultHeapProps = { D3D12_HEAP_TYPE_UPLOAD };
    vertexBuffer = device->createCommittedResource(defaultHeapProps, D3D12_HEAP_FLAG_NONE, vertexBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr);
    
    // Map and Upload data
    uint8_t* vertexMapPtr = nullptr;
    vertexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&vertexMapPtr));
    std::memcpy(vertexMapPtr, triangleVertices, sizeof(triangleVertices));
    vertexBuffer->Unmap(0, nullptr);
}
```

#### Draw
```cpp
void draw() {
    // Get and reset allocator for current frame, then point command list to it
    commandAllocators[frameIndex]->Reset();
    commandList->Reset(commandAllocators[frameIndex].get(), nullptr);
    {
        // Present->RenderTarget barrier ...
    
        D3D12_CPU_DESCRIPTOR_HANDLE frameRtvHandle = { rtvHandle.ptr + frameIndex * heapDescriptorSize };
        D3D12_VIEWPORT viewport = { 0, 0, windowProp.width, windowProp.height, D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
        D3D12_RECT scissorRect = { 0, 0, windowProp.width, windowProp.height };
    
        commandList->SetPipelineState(pipelineState.get());
        commandList->OMSetRenderTargets(1, &frameRtvHandle, FALSE, &dsvHandle);
        commandList->RSSetViewports(1, &viewport);
        commandList->RSSetScissorRects(1, &scissorRect);
    
        commandList->ClearRenderTargetView(frameRtvHandle, kClearRenderTarget.Color, 0, nullptr);
        commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH,
            kClearDepth.DepthStencil.Depth, kClearDepth.DepthStencil.Stencil, 0, nullptr);
    
        // Draw Triangle
        commandList->SetGraphicsRootSignature(pipelineRootSignature.get());
        commandList->SetGraphicsRootShaderResourceView(0, vertexBuffer->GetGPUVirtualAddress());
        commandList->DrawInstanced(3, 1, 0, 0);
    
        // RenderTarget->Present barrier ...
    } 
    commandList->Close();
    
    // Dispatch, Present, Wait
    ID3D12CommandList* commandLists[] = { commandList.get() };
    commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);
    swapChain->Present(1, 0);
    waitGpu();
}
```
