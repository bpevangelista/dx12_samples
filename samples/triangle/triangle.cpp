#define FASTDX_IMPLEMENTATION
#include "../../fastdx/fastdx.h"
#include <filesystem>
#include <fstream>

const int32_t kFrameCount = 3;
const DXGI_FORMAT kFrameFormat = DXGI_FORMAT_R10G10B10A2_UNORM;
const D3D12_CLEAR_VALUE kClearDepth = { DXGI_FORMAT_D32_FLOAT, {1.0f, 0} };
const D3D12_CLEAR_VALUE kClearRenderTarget = { kFrameFormat, { 0.0f, 0.2f, 0.4f, 1.0f } };
fastdx::WindowProperties windowProp;

fastdx::D3D12DeviceWrapperPtr device;
fastdx::ID3D12CommandQueuePtr commandQueue;
fastdx::ID3D12CommandAllocatorPtr commandAllocators[kFrameCount];
fastdx::ID3D12GraphicsCommandListPtr commandList;
fastdx::IDXGISwapChainPtr swapChain;
fastdx::ID3D12DescriptorHeapPtr swapChainRtvHeap;
fastdx::ID3D12DescriptorHeapPtr depthStencilViewHeap;
fastdx::ID3D12PipelineStatePtr pipelineState;
fastdx::ID3D12RootSignaturePtr pipelineRootSignature;
std::vector<fastdx::ID3D12ResourcePtr> renderTargets;
fastdx::ID3D12ResourcePtr depthStencilTarget;
std::vector<uint8_t> vertexShader, pixelShader;
fastdx::ID3D12ResourcePtr vertexBuffer;

int32_t frameIndex = 0;
HANDLE fenceEvent;
fastdx::ID3D12FencePtr swapFence;
uint64_t swapFenceCounter = 0;
uint64_t swapFenceWaitValue[kFrameCount] = {};

HRESULT readShader(const std::wstring& filePath, std::vector<uint8_t>& outShaderData) {
    WCHAR modulePathBuffer[1024];
    GetModuleFileName(nullptr, modulePathBuffer, _countof(modulePathBuffer));
    auto fullFilePath = std::filesystem::path(modulePathBuffer).parent_path() / filePath;

    std::ifstream file(fullFilePath, std::ios::binary);
    if (file) {
        std::uintmax_t fileSize = std::filesystem::file_size(fullFilePath);
        outShaderData.resize(fileSize);
        file.read(reinterpret_cast<char*>(outShaderData.data()), fileSize);
    }
    return file ? S_OK : E_FAIL;
}

void initializeD3d(HWND hwnd) {
    // Create a device and queue to dispatch command lists
    device = fastdx::createDevice(D3D_FEATURE_LEVEL_12_2);
    commandQueue = device->createCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);

    // Create a triple frame buffer swap chain for window
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = fastdx::defaultSwapChainDesc(hwnd, kFrameCount, kFrameFormat);
    swapChain = device->createSwapChainForHwnd(commandQueue, swapChainDesc, hwnd);

    // Create a heap of descriptors, then them fill with swap chain render targets desc
    swapChainRtvHeap = device->createHeapDescriptor(kFrameCount, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    renderTargets = device->createRenderTargetViews(swapChain, swapChainRtvHeap);

    // Create depth stencil resource
    D3D12_HEAP_PROPERTIES defaultHeapProps = { D3D12_HEAP_TYPE_DEFAULT };
    D3D12_RESOURCE_DESC depthStencilResourceDesc = fastdx::defaultResourceTexDesc(D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        swapChainDesc.Width, swapChainDesc.Height, 1, DXGI_FORMAT_D32_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

    depthStencilTarget = device->createCommittedResource(defaultHeapProps, D3D12_HEAP_FLAG_NONE,
        depthStencilResourceDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &kClearDepth);

    // Create heap descriptor with depth stencil desc
    D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc = {};
    depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
    depthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

    depthStencilViewHeap = device->createHeapDescriptor(1, D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    device->createDepthStencilView(depthStencilTarget, depthStencilDesc,
        depthStencilViewHeap->GetCPUDescriptorHandleForHeapStart());

    // Create one command allocator per frame buffer
    for (int32_t i = 0; i < kFrameCount; ++i) {
        commandAllocators[i] = device->createCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT);
    }

    // Single command list will reuse all allocators
    commandList = device->createCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocators[0]);
    commandList->Close();

    // Fence to wait for a completed frame to reuse
    swapFence = device->createFence(swapFenceCounter++, D3D12_FENCE_FLAG_NONE);
    fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    // Read VS, PS and Create root signature for shader
    readShader(L"simple_vs.cso", vertexShader);
    readShader(L"simple_ps.cso", pixelShader);
    pipelineRootSignature = device->createRootSignature(0, vertexShader.data(), vertexShader.size());

    // Create a pipeline state
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc = fastdx::defaultGraphicsPipelineDesc(kFrameFormat);
    pipelineDesc.pRootSignature = pipelineRootSignature.get();
    pipelineDesc.VS = { vertexShader.data(), vertexShader.size() };
    pipelineDesc.PS = { pixelShader.data(), pixelShader.size() };
    pipelineState = device->createGraphicsPipelineState(pipelineDesc);
}

void createTriangle() {
    struct Vertex { float x, y, z, w; float r, g, b, a; };
    Vertex triangleVertices[] = {
        { -0.8f, -0.8f, 0.0f, 1.0f,        1.0f, 0.0f, 0.0f, 1.0f, },
        {  0.0f,  0.8f, 0.0f, 1.0f,        0.0f, 1.0f, 0.0f, 1.0f, },
        {  0.8f, -0.8f, 0.0f, 1.0f,        0.0f, 0.0f, 1.0f, 1.0f, },
    };

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

void waitGpu(bool forceWait = false) {
    // Queue always signal increasing counter values
    commandQueue->Signal(swapFence.get(), swapFenceCounter);
    swapFenceWaitValue[frameIndex] = swapFenceCounter++;

    // Wait if next frame not ready
    int32_t nextFrameIndex = swapChain->GetCurrentBackBufferIndex();
    if (swapFence->GetCompletedValue() < swapFenceWaitValue[nextFrameIndex] || forceWait) {
        swapFence->SetEventOnCompletion(swapFenceWaitValue[nextFrameIndex], fenceEvent);
        WaitForSingleObjectEx(fenceEvent, INFINITE, FALSE);
    }
    frameIndex = nextFrameIndex;
}

void draw() {
    static D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = swapChainRtvHeap->GetCPUDescriptorHandleForHeapStart();
    static D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = depthStencilViewHeap->GetCPUDescriptorHandleForHeapStart();
    static size_t heapDescriptorSize = device->d3dDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE frameRtvHandle = { rtvHandle.ptr + frameIndex * heapDescriptorSize };

    static D3D12_RESOURCE_BARRIER transitionBarrier = { D3D12_RESOURCE_BARRIER_TYPE_TRANSITION, D3D12_RESOURCE_BARRIER_FLAG_NONE,
        nullptr,  D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES };

    // Get and reset allocator for current frame, then point command list to it
    auto commandAllocator = commandAllocators[frameIndex];
    commandAllocator->Reset();
    commandList->Reset(commandAllocator.get(), nullptr);
    {
        // Present->RenderTarget barrier
        transitionBarrier.Transition.pResource = renderTargets[frameIndex].get();
        transitionBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        transitionBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        commandList->ResourceBarrier(1, &transitionBarrier);

        D3D12_VIEWPORT viewport = { 0, 0, static_cast<float>(windowProp.width), static_cast<float>(windowProp.height),
            D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
        D3D12_RECT scissorRect = { 0, 0, windowProp.width, windowProp.height };

        commandList->SetPipelineState(pipelineState.get());
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        commandList->RSSetViewports(1, &viewport);
        commandList->RSSetScissorRects(1, &scissorRect);
        commandList->OMSetRenderTargets(1, &frameRtvHandle, FALSE, &dsvHandle);

        commandList->ClearRenderTargetView(frameRtvHandle, kClearRenderTarget.Color, 0, nullptr);
        commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH,
            kClearDepth.DepthStencil.Depth, kClearDepth.DepthStencil.Stencil, 0, nullptr);

        // Draw Triangle
        commandList->SetGraphicsRootSignature(pipelineRootSignature.get());
        commandList->SetGraphicsRootShaderResourceView(0, vertexBuffer->GetGPUVirtualAddress());
        commandList->DrawInstanced(3, 1, 0, 0);

        // RenderTarget->Present barrier
        transitionBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        transitionBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        commandList->ResourceBarrier(1, &transitionBarrier);
    }
    commandList->Close();

    // Dispatch command list and present
    ID3D12CommandList* commandLists[] = { commandList.get() };
    commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);
    swapChain->Present(1, 0);

    waitGpu();
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    HWND hwnd = fastdx::createWindow(windowProp);
    fastdx::onWindowDestroy = []() {
        waitGpu(true);
    };
    initializeD3d(hwnd);
    createTriangle();

    return fastdx::runMainLoop(nullptr, draw);
}
