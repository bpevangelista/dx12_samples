#include "../../fastdx/fastdx.h"

#include <filesystem>
#include <fstream>
#include <vector>
namespace fs = std::filesystem;

const int32_t kBufferCount = 3;
const DXGI_FORMAT kRenderTargetFormat = DXGI_FORMAT_R10G10B10A2_UNORM;

fastdx::D3D12DeviceWrapperPtr device;
fastdx::ID3D12CommandQueuePtr commandQueue;
fastdx::IDXGISwapChainPtr swapChain;
fastdx::ID3D12DescriptorHeapPtr swapChainRtvHeap;
//fastdx::ID3D12DescriptorHeapPtr depthStencilViewHeap;
fastdx::ID3D12DescriptorHeapPtr shaderResourceViewHeap;
std::vector<fastdx::ID3D12ResourcePtr> renderTargets;

fastdx::ID3D12CommandAllocatorPtr commandAllocators[kBufferCount];
fastdx::ID3D12GraphicsCommandListPtr commandList;
fastdx::ID3D12PipelineStatePtr pipelineState;
std::vector<uint8_t> vertexShader, pixelShader;

int32_t frameIndex = 0;
HANDLE fenceEvent;
fastdx::ID3D12FencePtr swapFence;
uint64_t swapFenceCounter = 0;
uint64_t swapFenceWaitValue[kBufferCount] = {};


HRESULT initializeD3d() {
    device = fastdx::createDevice(D3D_FEATURE_LEVEL_12_2);
    commandQueue = device->createCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
    swapChain = device->createWindowSwapChain(commandQueue, kBufferCount, kRenderTargetFormat);

    swapChainRtvHeap = device->createHeapDescriptor(16, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    //depthStencilViewHeap = device->createHeapDescriptor(1, D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    renderTargets = device->createRenderTargetViews(swapChain, swapChainRtvHeap);

    for (int32_t i = 0; i < kBufferCount; ++i) {
        commandAllocators[i] = device->createCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT);
    }

    commandList = device->createCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocators[0]);
    commandList->Close();

    swapFence = device->createFence(swapFenceCounter++, D3D12_FENCE_FLAG_NONE);
    fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (fenceEvent == nullptr) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    return S_OK;
}

HRESULT readShader(LPCWSTR filePath, std::vector<uint8_t>& outShaderData) {
    WCHAR modulePathBuffer[1024];
    GetModuleFileName(nullptr, modulePathBuffer, _countof(modulePathBuffer));
    
    fs::path modulePath = modulePathBuffer;
    auto fullFilePath = modulePath.parent_path() / filePath;
    std::ifstream file(fullFilePath, std::ios::binary);
    if (file) {
        std::uintmax_t fileSize = fs::file_size(fullFilePath);
        outShaderData.resize(fileSize);
        file.read(reinterpret_cast<char*>(outShaderData.data()), fileSize);
        file.close();
        return S_OK;
    }
    return E_FAIL;
}

#include <wrl.h> // Windows Runtime Library
Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;

HRESULT initializeAssets() {
    readShader(L"simple_vs.cso", vertexShader);
    readShader(L"simple_ps.cso", pixelShader);

    device->d3dDevice()->CreateRootSignature(0, vertexShader.data(), vertexShader.size(), IID_PPV_ARGS(&rootSignature));

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc = {};
    pipelineDesc.InputLayout = { nullptr, 0 };
    pipelineDesc.pRootSignature = rootSignature.Get();
    pipelineDesc.VS = { vertexShader.data(), vertexShader.size()};
    pipelineDesc.PS = { pixelShader.data(), pixelShader.size() };
    pipelineDesc.RasterizerState = fastdx::defaultRasterizerDesc();
    pipelineDesc.BlendState = fastdx::defaultBlendDesc();
    pipelineDesc.DepthStencilState.DepthEnable = FALSE;
    pipelineDesc.DepthStencilState.StencilEnable = FALSE;
    pipelineDesc.SampleMask = UINT_MAX;
    pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipelineDesc.NumRenderTargets = 1;
    pipelineDesc.RTVFormats[0] = kRenderTargetFormat;
    pipelineDesc.SampleDesc.Count = 1;
    pipelineState = device->createGraphicsPipelineState(pipelineDesc);
    
    //D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc = fastdx::defaultGraphicsPipelineState();
    //pipelineDesc.pRootSignature = nullptr;
    //pipelineDesc.VS = { nullptr, 0 };
    //pipelineDesc.PS = { nullptr, 0 };
    //pipelineDesc.RTVFormats[0] = kRenderTargetFormat;
    //pipelineState = device->createGraphicsPipelineState(pipelineDesc);

    return S_OK;
}

void update(double elapsedTimeMs) {

}

void draw() {
    auto commandAllocator = commandAllocators[frameIndex];
    commandAllocator->Reset();
    commandList->Reset(commandAllocator.get(), nullptr);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = swapChainRtvHeap->GetCPUDescriptorHandleForHeapStart();
    size_t heapDescriptorSize = device->d3dDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    rtvHandle.ptr += frameIndex * heapDescriptorSize;

    D3D12_RESOURCE_BARRIER transitionBarrier = { D3D12_RESOURCE_BARRIER_TYPE_TRANSITION, D3D12_RESOURCE_BARRIER_FLAG_NONE };
    transitionBarrier.Transition.pResource = renderTargets[frameIndex].get();
    transitionBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    {
        commandList->SetGraphicsRootSignature(rootSignature.Get());
        commandList->SetPipelineState(pipelineState.get());

        // Present<->RenderTarget barrier
        transitionBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        transitionBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        commandList->ResourceBarrier(1, &transitionBarrier);

        commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
        
        const float kClearColor[4] = { 0.0f, 0.2f, 0.4f, 1.0f };
        commandList->ClearRenderTargetView(rtvHandle, kClearColor, 0, nullptr);

        // Present<->RenderTarget barrier
        transitionBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        transitionBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        commandList->ResourceBarrier(1, &transitionBarrier);
    }

    commandList->Close();

    ID3D12CommandList* commandLists[] = { commandList.get() };
    commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);
    //swapChain->Present(0, DXGI_PRESENT_ALLOW_TEARING);
    swapChain->Present(1, 0);

    // Always signal increasing counter value
    commandQueue->Signal(swapFence.get(), swapFenceCounter);
    swapFenceWaitValue[frameIndex] = swapFenceCounter++;

    // Wait if next frame not ready
    int32_t nextFrameIndex = swapChain->GetCurrentBackBufferIndex();
    if (swapFence->GetCompletedValue() < swapFenceWaitValue[nextFrameIndex]) {
        swapFence->SetEventOnCompletion(swapFenceWaitValue[nextFrameIndex], fenceEvent);
        WaitForSingleObjectEx(fenceEvent, INFINITE, FALSE);
    }
    frameIndex = nextFrameIndex;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    fastdx::WindowProperties prop;
    fastdx::createWindow(prop);
    initializeD3d();
    initializeAssets();

    return fastdx::runMainLoop(update, draw);
}
