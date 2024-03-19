#define FASTDX_IMPLEMENTATION
#include "../../fastdx/fastdx.h"
#include "tiny_gltf/tiny_gltf.h"
#include <DirectXMath.h>
#include <filesystem>
#include <fstream>
using namespace std;

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
vector<fastdx::ID3D12ResourcePtr> renderTargets;
fastdx::ID3D12ResourcePtr depthStencilTarget;
vector<uint8_t> vertexShader, pixelShader;
fastdx::ID3D12ResourcePtr sceneConstantBuffer;
std::vector<fastdx::ID3D12ResourcePtr> uploadBuffers;

// Frame Sync
int32_t frameIndex = 0;
HANDLE fenceEvent;
fastdx::ID3D12FencePtr swapFence;
uint64_t swapFenceCounter = 0;
uint64_t swapFenceWaitValue[kFrameCount] = {};

// GlTF Model
vector<fastdx::ID3D12ResourcePtr> gltfVertexBuffers, gltfIndexBuffers;
vector<D3D12_INDEX_BUFFER_VIEW> gltfIndexBuffersView;
vector<vector<fastdx::ID3D12ResourcePtr>> gltfMaterialToTextures;
vector<D3D12_GPU_DESCRIPTOR_HANDLE> gltfTextureDescriptorsHeapStart;
fastdx::ID3D12DescriptorHeapPtr gltfTexturesViewHeap;

// Scene Constant Buffer
struct SceneGlobals { // On x64 we can guarantee 16B alignment
    DirectX::XMMATRIX matW;
    DirectX::XMMATRIX matVP;
};
SceneGlobals sceneGlobals = {};


void memcpyToInterleaved(uint8_t* dest, size_t destStrideInBytes, const uint8_t* src, size_t srcStrideInBytes, size_t srcSizeInBytes) {
    assert(srcSizeInBytes % srcStrideInBytes == 0);
    while (srcSizeInBytes > 0) {
        memcpy(dest, src, srcStrideInBytes);
        dest += destStrideInBytes;
        src += srcStrideInBytes;
        srcSizeInBytes -= srcStrideInBytes;
    }
}

wstring getPathInModule(const wstring& filePath) {
    WCHAR modulePathBuffer[2048];
    GetModuleFileName(nullptr, modulePathBuffer, _countof(modulePathBuffer));
    return filesystem::path(modulePathBuffer).parent_path() / filePath;
}

bool readGltfModel(const wstring& filePath, tinygltf::Model* outModel) {
    tinygltf::TinyGLTF loader;
    wstring warn, err;
    bool isLoaded = loader.LoadASCIIFromFile(outModel, &err, &warn, getPathInModule(filePath));
    if (!warn.empty() || !err.empty()) {
        OutputDebugString(warn.c_str());
        OutputDebugString(err.c_str());
    }
    return isLoaded;
}

HRESULT readShader(const wstring& filePath, vector<uint8_t>& outShaderData) {
    auto fullFilePath = getPathInModule(filePath);
    ifstream file(fullFilePath, ios::binary);
    if (file) {
        uintmax_t fileSize = filesystem::file_size(fullFilePath);
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

fastdx::ID3D12ResourcePtr createTextureBufferResource(const D3D12_RESOURCE_DESC& textureDesc, const void* dataPtr,
    int32_t rowSizeInBytes, int32_t sizeInBytes) {

    // Intermediate buffer with HEAP_TYPE_UPLOAD CPU->GPU
    D3D12_HEAP_PROPERTIES uploadHeapProps = { D3D12_HEAP_TYPE_UPLOAD };
    fastdx::ID3D12ResourcePtr cpuToGpuResource = device->createCommittedResource(uploadHeapProps,
        D3D12_HEAP_FLAG_NONE, fastdxu::resourceBufferDesc(sizeInBytes), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr);

    // Map and Upload data
    uint8_t* dataMapPtr = nullptr;
    cpuToGpuResource->Map(0, nullptr, reinterpret_cast<void**>(&dataMapPtr));
    memcpy(dataMapPtr, dataPtr, sizeInBytes);
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

// TODO Allow non-GPU upload
fastdx::ID3D12ResourcePtr createBufferResource(const void* dataPtr, int32_t sizeInBytes, D3D12_RESOURCE_STATES bufferState,
    D3D12_HEAP_TYPE heapType) {
    // Create D3D12 resource used for CPU to GPU upload
    D3D12_RESOURCE_DESC bufferDesc = fastdxu::resourceBufferDesc(sizeInBytes);
    D3D12_HEAP_PROPERTIES uploadHeapProps = { D3D12_HEAP_TYPE_UPLOAD };
    fastdx::ID3D12ResourcePtr cpuToGpuResource = device->createCommittedResource(uploadHeapProps,
        D3D12_HEAP_FLAG_NONE, bufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr);

    // Map and Upload data
    uint8_t* dataMapPtr = nullptr;
    cpuToGpuResource->Map(0, nullptr, reinterpret_cast<void**>(&dataMapPtr));
    memcpy(dataMapPtr, dataPtr, sizeInBytes);
    cpuToGpuResource->Unmap(0, nullptr);

    // CPU/GPU Managed Heap
    if (heapType == D3D12_HEAP_TYPE_UPLOAD) {
        return cpuToGpuResource;
    // GPU-Only Optimized Heap
    } else if (heapType == D3D12_HEAP_TYPE_DEFAULT) {
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
    // Not supported
    else {
        return nullptr;
    }
}

void createSceneConstantBuffer() {
    DirectX::XMFLOAT3 eye(0.0f, 5.0f, -10.0f);
    DirectX::XMFLOAT3 lookAt(0.0f, 0.0f, 0.0f);
    DirectX::XMFLOAT3 upVec(0.0f, 1.0f, 0.0f);
    auto matView = DirectX::XMMatrixLookAtLH(XMLoadFloat3(&eye), XMLoadFloat3(&lookAt), XMLoadFloat3(&upVec));
    auto matProj = DirectX::XMMatrixPerspectiveFovLH(DirectX::XM_PI / 3.0f, windowProp.width / (float)windowProp.height, 0.1f, 1000.0f);

    uint32_t cbSizeInBytes = sizeof(sceneGlobals);
    sceneGlobals.matW = DirectX::XMMatrixIdentity();
    sceneGlobals.matVP = DirectX::XMMatrixTranspose(matView * matProj); // HLSL expects column-major

    // Create constant buffer resource and its view for shader
    sceneConstantBuffer = createBufferResource(&sceneGlobals, cbSizeInBytes,
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_HEAP_TYPE_UPLOAD);
}

/// Return one VB/IB pair for each mesh part of each mesh
void loadGltfModelMeshes(const tinygltf::Model& gltfModel, vector<fastdx::ID3D12ResourcePtr>& outVertexBuffers,
    vector<fastdx::ID3D12ResourcePtr>& outIndexBuffers, vector<D3D12_INDEX_BUFFER_VIEW>& outIndexBuffersView) {

    vector<const tinygltf::Mesh*> meshes;
    for (const auto &scene : gltfModel.scenes) {
        for (auto sceneNodeId : scene.nodes) {
            const auto &modelNode = gltfModel.nodes[sceneNodeId];

            if (modelNode.mesh >= 0) {
                auto modelMeshId = modelNode.mesh;
                const auto& modelMesh = gltfModel.meshes[modelMeshId];
                meshes.push_back(&modelMesh);
            }
        }
    }

    // Vertex Buffers (XYZ, NxNyNz, UV) - Optionally, 16B align with pad
    int32_t vbStrideInBytes = (3 + 3 + 2) * sizeof(float);
    int32_t ibStrideInBytes = sizeof(uint16_t);

    for (const auto* mesh : meshes) {
        // Each meshParh must have a VB/IB pair
        for (auto meshPart : mesh->primitives) {
            uint8_t* vbDataPtr = nullptr;
            int32_t vbNumElements = 0;

            for (const auto& attrib : meshPart.attributes) {
                auto attribName = attrib.first;
                if (attribName != "POSITION" && attribName != "NORMAL" && attribName != "TEXCOORD_0") {
                    continue;
                }

                auto attribAccessor = gltfModel.accessors[attrib.second];
                assert(attribAccessor.byteOffset == 0);

                auto attribBufferView = gltfModel.bufferViews[attribAccessor.bufferView];
                const uint8_t* attribDataPtr = gltfModel.buffers[attribBufferView.buffer].data.data() +
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

            auto indexAccessor = gltfModel.accessors[meshPart.indices];
            auto bufferView = gltfModel.bufferViews[indexAccessor.bufferView];
            const uint8_t* indexDataPtr = gltfModel.buffers[bufferView.buffer].data.data() + bufferView.byteOffset;

            int32_t ibStrideInBytes = indexAccessor.ByteStride(bufferView);
            assert(ibStrideInBytes == sizeof(uint16_t));
            int32_t ibNumElements = static_cast<int32_t>(indexAccessor.count);
            uint8_t* ibDataPtr = (uint8_t*)malloc(ibNumElements * ibStrideInBytes);
            memcpy(ibDataPtr, indexDataPtr, ibNumElements * ibStrideInBytes);

            int32_t vbSizeInBytes = vbNumElements * vbStrideInBytes;
            int32_t ibSizeInBytes = ibNumElements * ibStrideInBytes;
            auto vertexBuffer = createBufferResource(vbDataPtr, vbSizeInBytes,
                D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_HEAP_TYPE_DEFAULT);
            auto indexBuffer = createBufferResource(ibDataPtr, ibSizeInBytes, D3D12_RESOURCE_STATE_INDEX_BUFFER,
                D3D12_HEAP_TYPE_DEFAULT);
            auto indexBufferView = fastdxu::indexBufferView(indexBuffer->GetGPUVirtualAddress(),
                ibNumElements * ibStrideInBytes, DXGI_FORMAT_R16_UINT);

            SAFE_FREE(vbDataPtr);
            SAFE_FREE(ibDataPtr);

            outVertexBuffers.push_back(vertexBuffer);
            outIndexBuffers.push_back(indexBuffer);
            outIndexBuffersView.push_back(indexBufferView);
        }
    }
}

void loadGltfModelMaterials(const tinygltf::Model& gltfModel,
    vector<vector<fastdx::ID3D12ResourcePtr>>& outMaterialToTextures,
    vector<D3D12_GPU_DESCRIPTOR_HANDLE>& outTextureDescriptorsHeapStart,
    fastdx::ID3D12DescriptorHeapPtr* outTexturesViewHeap) {

    map<int32_t, pair<D3D12_RESOURCE_DESC, fastdx::ID3D12ResourcePtr>> imageIdToTexture;
    vector<pair<D3D12_RESOURCE_DESC, fastdx::ID3D12ResourcePtr>> textureIdToTexture;

    for (int32_t textureId = 0; textureId < gltfModel.textures.size(); ++textureId) {
        const auto& texture = gltfModel.textures[textureId];
        int32_t imageId = texture.source;

        if (imageIdToTexture.find(imageId) == imageIdToTexture.end()) {
            const tinygltf::Image& image = gltfModel.images[imageId];
            assert(image.bits == 8);
            assert(image.component == 3 || image.component == 4); // R8G8B8 or R8G8B8A8

            // Create texture buffer
            auto imageDesc = fastdxu::resourceTexDesc(D3D12_RESOURCE_DIMENSION_TEXTURE2D,
                image.width, image.height, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_NONE);

            const void* imageDataPtr = &image.image[0];
            int32_t rowSizeInBytes = image.width * image.component;
            int32_t imageSizeInBytes = rowSizeInBytes * image.height;
            auto imageBuffer = createTextureBufferResource(imageDesc, imageDataPtr, rowSizeInBytes, imageSizeInBytes);
            imageIdToTexture[imageId] = { imageDesc, imageBuffer };
        }

        textureIdToTexture.push_back(imageIdToTexture[imageId]);
    }
    imageIdToTexture.clear();

    // Samplers - Not Implemented
    for (const auto& sampler : gltfModel.samplers) {
    }

    int32_t descriptorsCount = ((gltfModel.materials.size() * 5) + 31) & ~31;
    size_t descriptorSizeInBytes = device->getDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    fastdx::ID3D12DescriptorHeapPtr texturesViewHeap = device->createDescriptorHeap(
        descriptorsCount, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_CPU_DESCRIPTOR_HANDLE texturesCpuHandle = texturesViewHeap->GetCPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE texturesGpuHandle = texturesViewHeap->GetGPUDescriptorHandleForHeapStart();

    for (auto material : gltfModel.materials) {
        int32_t textureIds[] = {
            material.pbrMetallicRoughness.baseColorTexture.index
            , material.pbrMetallicRoughness.metallicRoughnessTexture.index
            //, material.normalTexture.index        // Not supported
            //, material.emissiveTexture.index      // Not supported
            //, material.occlusionTexture.index     // Not supported
        };
        outTextureDescriptorsHeapStart.push_back(texturesGpuHandle);

        vector<fastdx::ID3D12ResourcePtr> texturesPtr;
        for (int32_t i=0; i <_countof(textureIds); ++i) {
            int32_t textureId = textureIds[i];

            assert(textureId != -1, "Missing required texture");
            if (textureId == -1) {
                // TODO Gracefully handle with default albedo and normal textures
                continue;
            }

            const auto& textureDescAndPtr = textureIdToTexture[textureId];
            const D3D12_RESOURCE_DESC& textureDesc = textureDescAndPtr.first;
            const fastdx::ID3D12ResourcePtr& texturePtr = textureDescAndPtr.second;

            D3D12_SHADER_RESOURCE_VIEW_DESC imageViewDesc = fastdxu::shaderResourceViewDesc(
                D3D12_SRV_DIMENSION_TEXTURE2D, textureDesc.Format);
            imageViewDesc.Texture2D.MipLevels = textureDesc.MipLevels + 1;

            device->createShaderResourceView(texturePtr, imageViewDesc, texturesCpuHandle);
            texturesCpuHandle.ptr += descriptorSizeInBytes;
            texturesGpuHandle.ptr += descriptorSizeInBytes;

            texturesPtr.push_back(texturePtr);
        }
        outMaterialToTextures.push_back(std::move(texturesPtr));
    }
    *outTexturesViewHeap = texturesViewHeap;
}

void update(float elapsedTimeSec) {
    static float angleY = 0.0f;
    angleY += elapsedTimeSec * 0.001f;
    sceneGlobals.matW = DirectX::XMMatrixRotationY(angleY);

    uint8_t* dataMapPtr = nullptr;
    sceneConstantBuffer->Map(0, nullptr, reinterpret_cast<void**>(&dataMapPtr));
    memcpy(dataMapPtr, &sceneGlobals, sizeof(sceneGlobals));
    sceneConstantBuffer->Unmap(0, nullptr);
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

        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        commandList->SetGraphicsRootSignature(pipelineRootSignature.get());
        commandList->SetGraphicsRootConstantBufferView(0, sceneConstantBuffer->GetGPUVirtualAddress());

        // Draw all mesh parts
        ID3D12DescriptorHeap* shaderTexturesHeaps[] = { gltfTexturesViewHeap.get() };
        commandList->SetDescriptorHeaps(1, shaderTexturesHeaps);
        for (int i = 0; i < gltfIndexBuffers.size(); ++i) {
            commandList->IASetIndexBuffer(&gltfIndexBuffersView[i]);
            commandList->SetGraphicsRootShaderResourceView(1, gltfVertexBuffers[i]->GetGPUVirtualAddress());

            // Textures must use descriptor table
            commandList->SetGraphicsRootDescriptorTable(2, gltfTextureDescriptorsHeapStart[i]);
            commandList->DrawIndexedInstanced(gltfIndexBuffersView[i].SizeInBytes / sizeof(uint16_t), 1, 0, 0, 0);
        }

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
        tinygltf::Model gltfCubeModel;
        readGltfModel(L"Cube.gltf", &gltfCubeModel);
        loadGltfModelMeshes(gltfCubeModel, gltfVertexBuffers, gltfIndexBuffers, gltfIndexBuffersView);
        loadGltfModelMaterials(gltfCubeModel, gltfMaterialToTextures, gltfTextureDescriptorsHeapStart, &gltfTexturesViewHeap);

        createSceneConstantBuffer();
    }
    executeCommandList();
    waitGpu(true);
    uploadBuffers.clear();

    return fastdx::runMainLoop(update, draw);
}
