#define FASTDX_IMPLEMENTATION
#include "../../fastdx/fastdx.h"
#include "tiny_gltf.h"
#include <DirectXMath.h>
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
fastdx::ID3D12DescriptorHeapPtr texturesViewHeap;
fastdx::ID3D12PipelineStatePtr pipelineState;
fastdx::ID3D12RootSignaturePtr pipelineRootSignature;
std::vector<fastdx::ID3D12ResourcePtr> renderTargets;
fastdx::ID3D12ResourcePtr depthStencilTarget;
std::vector<uint8_t> vertexShader, pixelShader;
fastdx::ID3D12ResourcePtr vertexBuffer, indexBuffer, constantBuffer;
std::vector<fastdx::ID3D12ResourcePtr> textureBuffers;
std::vector<fastdx::ID3D12ResourcePtr> uploadBuffers;
D3D12_INDEX_BUFFER_VIEW indexBufferView;

int32_t frameIndex = 0;
HANDLE fenceEvent;
fastdx::ID3D12FencePtr swapFence;
uint64_t swapFenceCounter = 0;
uint64_t swapFenceWaitValue[kFrameCount] = {};

struct SceneGlobals { // On x64 we can guarantee 16B alignment
    DirectX::XMMATRIX matW;
    DirectX::XMMATRIX matVP;
};

tinygltf::Model gltfCubeModel;
SceneGlobals sceneGlobals = {};


void memcpyToInterleaved(uint8_t* dest, size_t destStrideInBytes, uint8_t* src, size_t srcStrideInBytes, size_t srcSizeInBytes) {
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

    // Create heaps for render target views, depth stencil and shader parameters
    swapChainRtvHeap = device->createDescriptorHeap(kFrameCount, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    depthStencilViewHeap = device->createDescriptorHeap(1, D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    texturesViewHeap = device->createDescriptorHeap(32, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // Create a triple frame buffer swap chain for window
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = fastdxu::swapChainDesc(hwnd, kFrameCount, kFrameFormat);
    swapChain = device->createSwapChainForHwnd(commandQueue, swapChainDesc, hwnd);

    // Create swap chain render targets views in heap
    renderTargets = device->createRenderTargetViews(swapChain, swapChainRtvHeap);

    // Create depth stencil resource
    D3D12_HEAP_PROPERTIES defaultHeapProps = { D3D12_HEAP_TYPE_DEFAULT };
    D3D12_RESOURCE_DESC depthStencilResourceDesc = fastdxu::resourceTexDesc(D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        swapChainDesc.Width, swapChainDesc.Height, 1, DXGI_FORMAT_D32_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

    depthStencilTarget = device->createCommittedResource(defaultHeapProps, D3D12_HEAP_FLAG_NONE,
        depthStencilResourceDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &kClearDepth);

    // Create depth stencil render target view in heap
    D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc = {};
    depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
    depthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
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
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc = fastdxu::defaultGraphicsPipelineDesc(kFrameFormat);
    pipelineDesc.pRootSignature = pipelineRootSignature.get();
    pipelineDesc.VS = { vertexShader.data(), vertexShader.size() };
    pipelineDesc.PS = { pixelShader.data(), pixelShader.size() };
    pipelineState = device->createGraphicsPipelineState(pipelineDesc);
}

void startCommandList() {
    // Get and reset allocator for current frame, then point command list to it
    auto commandAllocator = commandAllocators[frameIndex];
    commandAllocator->Reset();
    commandList->Reset(commandAllocator.get(), nullptr);
}

void executeCommandList() {
    // Close and dispatch command
    commandList->Close();
    ID3D12CommandList* commandLists[] = { commandList.get() };
    commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);
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

fastdx::ID3D12ResourcePtr createTextureBufferResource(const D3D12_RESOURCE_DESC& textureDesc, void* dataPtr, int32_t rowSizeInBytes, int32_t sizeInBytes) {
    // Intermediate buffer with HEAP_TYPE_UPLOAD CPU->GPU
    D3D12_HEAP_PROPERTIES uploadHeapProps = { D3D12_HEAP_TYPE_UPLOAD };
    fastdx::ID3D12ResourcePtr cpuToGpuResource = device->createCommittedResource(uploadHeapProps,
        D3D12_HEAP_FLAG_NONE, fastdxu::resourceBufferDesc(sizeInBytes), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr);

    // Map and Upload data
    uint8_t* dataMapPtr = nullptr;
    cpuToGpuResource->Map(0, nullptr, reinterpret_cast<void**>(&dataMapPtr));
    std::memcpy(dataMapPtr, dataPtr, sizeInBytes);
    cpuToGpuResource->Unmap(0, nullptr);

    // Final GPU-read optimized buffer. Dispatch COPY command, HEAP_UPLOAD -> HEAP_DEFAULT
    D3D12_HEAP_PROPERTIES defaultHeapProps = { D3D12_HEAP_TYPE_DEFAULT };
    fastdx::ID3D12ResourcePtr resource = device->createCommittedResource(defaultHeapProps,
        D3D12_HEAP_FLAG_NONE, textureDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr);

    // Issue GPU CopyTextureRegion command
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT resourceFootprint;
    device->d3dDevice()->GetCopyableFootprints(&textureDesc, 0, 1, 0, &resourceFootprint, nullptr, nullptr, nullptr);

    uint32_t subresourceIndex = 0;
    D3D12_TEXTURE_COPY_LOCATION srcRegion = { cpuToGpuResource.get(), D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT, resourceFootprint };
    D3D12_TEXTURE_COPY_LOCATION dstRegion = { resource.get(), D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, subresourceIndex };
    commandList->CopyTextureRegion(&dstRegion, 0, 0, 0, &srcRegion, nullptr);

    // Transition Barrier
    D3D12_RESOURCE_BARRIER transitionBarrier = fastdxu::resourceBarrierTransition(resource,
        D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    commandList->ResourceBarrier(1, &transitionBarrier);

    uploadBuffers.push_back(cpuToGpuResource);
    return resource;
}

fastdx::ID3D12ResourcePtr createBufferResource(void* dataPtr, int32_t sizeInBytes, D3D12_RESOURCE_STATES bufferState) {
    // Create D3D12 resource used for CPU to GPU upload
    D3D12_RESOURCE_DESC bufferDesc = fastdxu::resourceBufferDesc(sizeInBytes);
    D3D12_HEAP_PROPERTIES uploadHeapProps = { D3D12_HEAP_TYPE_UPLOAD };
    fastdx::ID3D12ResourcePtr cpuToGpuResource = device->createCommittedResource(uploadHeapProps,
        D3D12_HEAP_FLAG_NONE, bufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr);

    // Map and Upload data
    uint8_t* dataMapPtr = nullptr;
    cpuToGpuResource->Map(0, nullptr, reinterpret_cast<void**>(&dataMapPtr));
    std::memcpy(dataMapPtr, dataPtr, sizeInBytes);
    cpuToGpuResource->Unmap(0, nullptr);

    // Create a read optimized GPU resource, copy from HEAP_UPLOAD to HEAP_DEFAULT resource
    D3D12_HEAP_PROPERTIES defaultHeapProps = { D3D12_HEAP_TYPE_DEFAULT };
    fastdx::ID3D12ResourcePtr resource = device->createCommittedResource(defaultHeapProps,
        D3D12_HEAP_FLAG_NONE, bufferDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr);

    // Issue GPU CopyResource command
    commandList->CopyResource(resource.get(), cpuToGpuResource.get());

    D3D12_RESOURCE_BARRIER transitionBarrier = fastdxu::resourceBarrierTransition(resource,
        D3D12_RESOURCE_STATE_COPY_DEST, bufferState);
    commandList->ResourceBarrier(1, &transitionBarrier);

    uploadBuffers.push_back(cpuToGpuResource);
    return resource;
}

void loadScene() {
    uint32_t cbSizeInBytes = sizeof(sceneGlobals);
    sceneGlobals.matW = DirectX::XMMatrixIdentity();
    sceneGlobals.matVP = DirectX::XMMatrixIdentity();

    // Create constant buffer resource and its view for shader
    constantBuffer = createBufferResource(&sceneGlobals, cbSizeInBytes, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
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

    std::vector<uint8_t*> vbBuffers;
    std::vector<int32_t> vbBuffersNumElements;
    // Vertex Buffers (XYZ, NxNyNz, UV) - Optionally, 16B align with pad
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

                auto attribAccessor = gltfCubeModel.accessors[attrib.second];
                assert(attribAccessor.byteOffset == 0);

                auto attribBufferView = gltfCubeModel.bufferViews[attribAccessor.bufferView];
                uint8_t* attribDataPtr = gltfCubeModel.buffers[attribBufferView.buffer].data.data() +
                    attribBufferView.byteOffset;

                // Create buffer on first attribute, then make sure they all have the same count
                if (vbDataPtr == nullptr) {
                    vbNumElements = static_cast<int32_t>(attribAccessor.count);
                    vbDataPtr = (uint8_t*)malloc(attribAccessor.count * vbStrideInBytes);
                    memset(vbDataPtr, 0, attribAccessor.count * vbStrideInBytes);
                }
                else {
                    assert(vbNumElements == attribAccessor.count);
                }

                uint8_t* vbCopyToPtr = vbDataPtr;
                if (attribName == "NORMAL") {
                    vbCopyToPtr += 3 * sizeof(float); // skip position
                }
                else if (attribName == "TEXCOORD_0") {
                    vbCopyToPtr += 6 * sizeof(float); // skip position and normal
                }

                int32_t attribStrideInBytes = attribAccessor.ByteStride(attribBufferView);
                memcpyToInterleaved(vbCopyToPtr, vbStrideInBytes, attribDataPtr, attribStrideInBytes, attribBufferView.byteLength);
            }

            auto indexAccessor = gltfCubeModel.accessors[meshPart.indices];
            auto bufferView = gltfCubeModel.bufferViews[indexAccessor.bufferView];
            uint8_t* indexDataPtr = gltfCubeModel.buffers[bufferView.buffer].data.data() + bufferView.byteOffset;

            int32_t ibStrideInBytes = indexAccessor.ByteStride(bufferView);
            assert(ibStrideInBytes == sizeof(uint16_t));
            int32_t ibNumElements = static_cast<int32_t>(indexAccessor.count);
            uint8_t* ibDataPtr = (uint8_t*)malloc(ibNumElements * ibStrideInBytes);

            memcpy(ibDataPtr, indexDataPtr, ibNumElements * ibStrideInBytes);

            ibBuffers.push_back(ibDataPtr);
            ibBuffersNumElements.push_back(ibNumElements);
            vbBuffers.push_back(vbDataPtr);
            vbBuffersNumElements.push_back(vbNumElements);
        }
    }

    // Must have at least one meshPart with vertex and index buffer
    assert(vbBuffers.size() > 0 && ibBuffers.size() > 0);
    int32_t vbSizeInBytes = vbBuffersNumElements[0] * vbStrideInBytes;
    int32_t ibSizeInBytes = ibBuffersNumElements[0] * ibStrideInBytes;
    vertexBuffer = createBufferResource(vbBuffers[0], vbSizeInBytes, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    indexBuffer = createBufferResource(ibBuffers[0], ibSizeInBytes, D3D12_RESOURCE_STATE_INDEX_BUFFER);
    indexBufferView = fastdxu::indexBufferView(indexBuffer->GetGPUVirtualAddress(),
        ibBuffersNumElements[0] * ibStrideInBytes, DXGI_FORMAT_R16_UINT);

    for (uint8_t* vbDataPtr : vbBuffers) {
        SAFE_FREE(vbDataPtr);
    }
    for (uint8_t* ibDataPtr : ibBuffers) {
        SAFE_FREE(ibDataPtr);
    }
}

void loadMaterials() {
    for (const auto& texture : gltfCubeModel.samplers) {
        // Handle Samplers
    }

    D3D12_CPU_DESCRIPTOR_HANDLE texturesHandle = texturesViewHeap->GetCPUDescriptorHandleForHeapStart();
    size_t heapDescriptorSize = device->getDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    std::map<int32_t, fastdx::ID3D12ResourcePtr> imagesMap;
    for (const auto& texture : gltfCubeModel.textures) {
        int32_t imageId = texture.source;
        if (imagesMap.find(imageId) == imagesMap.end()) {
            tinygltf::Image& image = gltfCubeModel.images[imageId];
            assert(image.bits == 8);
            assert(image.component == 3 || image.component == 4); // R8G8B8 or R8G8B8A8

            // Create texture buffer
            auto imageDesc = fastdxu::resourceTexDesc(D3D12_RESOURCE_DIMENSION_TEXTURE2D,
                image.width, image.height, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_NONE);

            void* imageDataPtr = &image.image[0];
            int32_t rowSizeInBytes = image.width * image.component;
            int32_t imageSizeInBytes = rowSizeInBytes * image.height;
            auto imageBuffer = createTextureBufferResource(imageDesc, imageDataPtr, rowSizeInBytes, imageSizeInBytes);
            textureBuffers.push_back(imageBuffer);

            // Create texture view descriptor
            D3D12_SHADER_RESOURCE_VIEW_DESC imageViewDesc = fastdxu::shaderResourceViewDesc(
                D3D12_SRV_DIMENSION_TEXTURE2D, imageDesc.Format);
            imageViewDesc.Texture2D.MipLevels = imageDesc.MipLevels + 1;

            device->createShaderResourceView(imageBuffer, imageViewDesc, texturesHandle);
            texturesHandle.ptr += heapDescriptorSize;
        }
    }
}

void draw() {
    static D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = swapChainRtvHeap->GetCPUDescriptorHandleForHeapStart();
    static D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = depthStencilViewHeap->GetCPUDescriptorHandleForHeapStart();
    static size_t heapDescriptorSize = device->getDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE frameRtvHandle = { rtvHandle.ptr + frameIndex * heapDescriptorSize };

    static D3D12_RESOURCE_BARRIER transitionBarrier = fastdxu::resourceBarrierTransition(nullptr);

    startCommandList();
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
        commandList->SetGraphicsRootConstantBufferView(0, constantBuffer->GetGPUVirtualAddress());
        commandList->SetGraphicsRootShaderResourceView(1, vertexBuffer->GetGPUVirtualAddress());

        // Textures must use descriptor table
        ID3D12DescriptorHeap* shaderTexturesHeaps[] = { texturesViewHeap.get() };
        commandList->SetDescriptorHeaps(1, shaderTexturesHeaps);
        commandList->SetGraphicsRootDescriptorTable(2, texturesViewHeap->GetGPUDescriptorHandleForHeapStart());
        commandList->DrawIndexedInstanced(indexBufferView.SizeInBytes / sizeof(uint16_t), 1, 0, 0, 0);

        // RenderTarget->Present barrier
        transitionBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        transitionBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        commandList->ResourceBarrier(1, &transitionBarrier);
    }
    executeCommandList();

    swapChain->Present(1, 0);
    waitGpu();
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    HWND hwnd = fastdx::createWindow(windowProp);
    fastdx::onWindowDestroy = []() {
        waitGpu(true);
    };
    initializeD3d(hwnd);

    startCommandList();
    {
        loadMeshes();
        loadMaterials();
        loadScene();
    }
    executeCommandList();
    waitGpu(true);

    return fastdx::runMainLoop(nullptr, draw);
}
