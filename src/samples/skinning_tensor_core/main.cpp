#include "../fastdx/fastdx.h"
using namespace Microsoft::WRL;

fastdx::ID3D12Device2Ptr device;
fastdx::ID3D12CommandQueuePtr commandQueue;
fastdx::ID3D12GraphicsCommandList6Ptr graphicsCommands;
fastdx::IDXGISwapChain3Ptr swapChain;

fastdx::ID3D12DescriptorHeapPtr renderTargetViewHeap;
fastdx::ID3D12DescriptorHeapPtr depthStencilViewHeap;
fastdx::ID3D12DescriptorHeapPtr shaderResourceViewHeap;

void initializeD3d() {
    device = fastdx::createDevice(D3D_FEATURE_LEVEL_12_2);
    commandQueue = fastdx::createCommandQueue(device, D3D12_COMMAND_LIST_TYPE_DIRECT);
    swapChain = fastdx::createWindowSwapChain(device, commandQueue, DXGI_FORMAT_R10G10B10A2_UNORM);

    renderTargetViewHeap = fastdx::createHeapDescriptor(device, 16, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    depthStencilViewHeap = fastdx::createHeapDescriptor(device, 1, D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    shaderResourceViewHeap = fastdx::createHeapDescriptor(device, 32, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    fastdx::createRenderTargetViews(device, swapChain, renderTargetViewHeap);
}


void update(float elapsedTimeMs) {

}


void draw() {
    ID3D12CommandList* commandLists[] = { graphicsCommands.get() };
    commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

    swapChain->Present(1, 0);
}


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {

    fastdx::WindowProperties prop;
    fastdx::createWindow(prop);

    initializeD3d();

    return fastdx::runMainLoop();
}
