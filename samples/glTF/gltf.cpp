#define FASTDX_IMPLEMENTATION
#include "../../fastdx/fastdx.h"
#include "tiny_gltf.h"
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
fastdx::ID3D12ResourcePtr vertexBuffer, indexBuffer;
D3D12_INDEX_BUFFER_VIEW indexBufferView;

int32_t frameIndex = 0;
HANDLE fenceEvent;
fastdx::ID3D12FencePtr swapFence;
uint64_t swapFenceCounter = 0;
uint64_t swapFenceWaitValue[kFrameCount] = {};

tinygltf::Model gltfCubeModel;


void memcpyToInterleaved(uint8_t* dest, size_t destStrideInBytes, uint8_t* src, size_t srcSizeInBytes, size_t srcStrideInBytes) {
    assert(srcSizeInBytes % srcStrideInBytes == 0);
    while (srcSizeInBytes > 0) {
        memcpy(dest, src, srcStrideInBytes);
        dest += destStrideInBytes;
        src += srcStrideInBytes;
        srcSizeInBytes -= srcStrideInBytes;
    }
}

std::wstring getPathInModule(const std::wstring& filePath) {
    WCHAR modulePathBuffer[2048];
    GetModuleFileName(nullptr, modulePathBuffer, _countof(modulePathBuffer));
    return std::filesystem::path(modulePathBuffer).parent_path() / filePath;
}

bool readModel(const std::wstring& filePath, tinygltf::Model* outModel) {
    tinygltf::TinyGLTF loader;

    std::wstring warn, err;
    bool isLoaded = loader.LoadASCIIFromFile(outModel, &err, &warn, getPathInModule(filePath));
    if (!warn.empty() || !err.empty()) {
        OutputDebugString(warn.c_str());
        OutputDebugString(err.c_str());
    }
    return isLoaded;
}

HRESULT readShader(const std::wstring& filePath, std::vector<uint8_t>& outShaderData) {
    auto fullFilePath = getPathInModule(filePath);
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
    readShader(L"textured_vs.cso", vertexShader);
    readShader(L"textured_ps.cso", pixelShader);
    pipelineRootSignature = device->createRootSignature(0, vertexShader.data(), vertexShader.size());

    // Create a pipeline state
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc = fastdx::defaultGraphicsPipelineDesc(kFrameFormat);
    pipelineDesc.pRootSignature = pipelineRootSignature.get();
    pipelineDesc.VS = { vertexShader.data(), vertexShader.size() };
    pipelineDesc.PS = { pixelShader.data(), pixelShader.size() };
    pipelineState = device->createGraphicsPipelineState(pipelineDesc);
}

fastdx::ID3D12ResourcePtr createBufferResource(uint8_t* dataPtr, int32_t sizeInBytes) {
    // Create resource on D3D12_HEAP_TYPE_UPLOAD (ideally, copy to D3D12_HEAP_DEFAULT)
    D3D12_RESOURCE_DESC vertexBufferDesc = fastdx::defaultResourceBufferDesc(sizeInBytes);
    D3D12_HEAP_PROPERTIES defaultHeapProps = { D3D12_HEAP_TYPE_UPLOAD };
    fastdx::ID3D12ResourcePtr resource = device->createCommittedResource(defaultHeapProps, D3D12_HEAP_FLAG_NONE, vertexBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr);

    // Map and Upload data
    uint8_t* dataMapPtr = nullptr;
    resource->Map(0, nullptr, reinterpret_cast<void**>(&dataMapPtr));
    std::memcpy(dataMapPtr, dataPtr, sizeInBytes);
    resource->Unmap(0, nullptr);
    return resource;
}

void loadMeshes() {
    readModel(L"Cube.gltf", &gltfCubeModel);

    std::vector<const tinygltf::Mesh*> meshes;
    for (const auto &scene : gltfCubeModel.scenes) {
        for (auto sceneNodeId : scene.nodes) {
            const auto &modelNode = gltfCubeModel.nodes[sceneNodeId];

            if (modelNode.mesh >= 0) {
                auto modelMeshId = modelNode.mesh;
                const auto& modelMesh = gltfCubeModel.meshes[modelMeshId];
                meshes.push_back(&modelMesh);
            }
        }
    }

    // Vertex Buffers (XYZ, NxNyNz, UV)
    std::vector<uint8_t*> vbBuffers;
    std::vector<int32_t> vbBuffersNumElements;
    int32_t vbStrideInBytes = (3 + 3 + 2) * sizeof(float);

    std::vector<uint8_t*> ibBuffers;
    std::vector<int32_t> ibBuffersNumElements;
    int32_t ibStrideInBytes = sizeof(uint16_t);

    for (const auto* mesh : meshes) {
        for (auto meshPart : mesh->primitives) {
            uint8_t* vbDataPtr = nullptr;
            int32_t vbNumElements = 0;

            for (const auto& attrib : meshPart.attributes) {
                auto attribName = attrib.first;
                if (attribName != "POSITION" && attribName != "NORMAL" && attribName != "TEXCOORD_0") {
                    continue;
                }

                auto accessor = gltfCubeModel.accessors[attrib.second];
                assert(accessor.byteOffset == 0);

                // Create buffer on first attribute, then make sure they all have the same count
                if (vbDataPtr == nullptr) {
                    vbNumElements = static_cast<int32_t>(accessor.count);
                    vbDataPtr = (uint8_t*)malloc(accessor.count * vbStrideInBytes);
                }
                else {
                    assert(vbNumElements == accessor.count);
                }

                auto bufferView = gltfCubeModel.bufferViews[accessor.bufferView];
                uint8_t* bufferDataPtr = gltfCubeModel.buffers[bufferView.buffer].data.data() + bufferView.byteOffset;

                uint8_t* vbAttribDataPtr = vbDataPtr;
                if (attribName == "NORMAL") {
                    vbAttribDataPtr += 3 * sizeof(float); // skip position
                }
                else if (attribName == "TEXCOORD_0") {
                    vbAttribDataPtr += 6 * sizeof(float); // skip position and normal
                }

                int32_t vbStrideInBytes = accessor.ByteStride(bufferView);
                memcpyToInterleaved(vbAttribDataPtr, vbStrideInBytes, bufferDataPtr, bufferView.byteLength, vbStrideInBytes);
            }

            auto indexAccessor = gltfCubeModel.accessors[meshPart.indices];
            auto bufferView = gltfCubeModel.bufferViews[indexAccessor.bufferView];
            uint8_t* bufferDataPtr = gltfCubeModel.buffers[bufferView.buffer].data.data() + bufferView.byteOffset;

            int32_t ibStrideInBytes = indexAccessor.ByteStride(bufferView);
            assert(ibStrideInBytes == sizeof(uint16_t));
            int32_t ibNumElements = static_cast<int32_t>(indexAccessor.count);
            uint8_t* ibDataPtr = (uint8_t*)malloc(ibNumElements * ibStrideInBytes);

            memcpy(ibDataPtr, bufferDataPtr, ibNumElements * ibStrideInBytes);

            ibBuffers.push_back(ibDataPtr);
            ibBuffersNumElements.push_back(ibNumElements);
            vbBuffers.push_back(vbDataPtr);
            vbBuffersNumElements.push_back(vbNumElements);
        }
    }

    // Must have at least one meshPart with vertex and index buffer
    assert(vbBuffers.size() > 0 && ibBuffers.size() > 0);
    vertexBuffer = createBufferResource(vbBuffers[0], vbBuffersNumElements[0] * vbStrideInBytes);
    indexBuffer = createBufferResource(ibBuffers[0], ibBuffersNumElements[0] * ibStrideInBytes);
    indexBufferView = fastdx::defaultIndexBufferView(indexBuffer->GetGPUVirtualAddress(),
        ibBuffersNumElements[0] * ibStrideInBytes, DXGI_FORMAT_R16_UINT);

    for (uint8_t* vbDataPtr : vbBuffers) {
        SAFE_FREE(vbDataPtr);
    }
    for (uint8_t* ibDataPtr : ibBuffers) {
        SAFE_FREE(ibDataPtr);
    }
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
        commandList->RSSetViewports(1, &viewport);
        commandList->RSSetScissorRects(1, &scissorRect);
        commandList->OMSetRenderTargets(1, &frameRtvHandle, FALSE, &dsvHandle);

        commandList->ClearRenderTargetView(frameRtvHandle, kClearRenderTarget.Color, 0, nullptr);
        commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH,
            kClearDepth.DepthStencil.Depth, kClearDepth.DepthStencil.Stencil, 0, nullptr);

        // Draw Mesh
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        commandList->IASetIndexBuffer(&indexBufferView);
        commandList->SetGraphicsRootSignature(pipelineRootSignature.get());
        //commandList->SetGraphicsRootConstantBufferView(0, nullptr);
        commandList->SetGraphicsRootShaderResourceView(1, vertexBuffer->GetGPUVirtualAddress());
        //commandList->SetGraphicsRootShaderResourceView(2, textureBuffer->GetGPUVirtualAddress());
        commandList->DrawIndexedInstanced(indexBufferView.SizeInBytes / sizeof(uint16_t), 1, 0, 0, 0);

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
    loadMeshes();

    return fastdx::runMainLoop(nullptr, draw);
}
