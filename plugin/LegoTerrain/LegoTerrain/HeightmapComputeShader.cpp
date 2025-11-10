#include "HeightmapComputeShader.h"
#include <maya/MGlobal.h>
#include <maya/MImage.h>
#include <maya/MOpenCLInfo.h>
#include <maya/MOpenCLAutoPtr.h>
#include <maya/MVector.h>
#include <vector>
#include <algorithm>
#include <cmath>

HeightmapComputeShader::HeightmapComputeShader()
    : fContext(nullptr)
    , fQueue(nullptr)
    , fInitialized(false)
{
}

HeightmapComputeShader::~HeightmapComputeShader()
{
    cleanup();
}

bool HeightmapComputeShader::isInitialized() const
{
    return fInitialized;
}

const char* HeightmapComputeShader::getKernelSource()
{
    return R"(
// Bilinear interpolation
float sampleHeight(__global uchar4* input, int imgWidth, int imgHeight, float u, float v, int maxHeight)
{
    u = clamp(u, 0.0f, (float)(imgWidth - 1));
    v = clamp(v, 0.0f, (float)(imgHeight - 1));

    int x0 = (int)floor(u);
    int y0 = (int)floor(v);
    int x1 = min(x0 + 1, imgWidth - 1);
    int y1 = min(y0 + 1, imgHeight - 1);
    
    float fx = u - (float)x0;
    float fy = v - (float)y0;

    // Corner sampling
    uchar4 p00 = input[y0 * imgWidth + x0];
    uchar4 p10 = input[y0 * imgWidth + x1];
    uchar4 p01 = input[y1 * imgWidth + x0];
    uchar4 p11 = input[y1 * imgWidth + x1];

    // Get grayscale heights
    float h00 = ((float)p00.x + (float)p00.y + (float)p00.z) / 3.0f;
    float h10 = ((float)p10.x + (float)p10.y + (float)p10.z) / 3.0f;
    float h01 = ((float)p01.x + (float)p01.y + (float)p01.z) / 3.0f;
    float h11 = ((float)p11.x + (float)p11.y + (float)p11.z) / 3.0f;

    // Bi linear interpolation
    float h0 = mix(h00, h10, fx);
    float h1 = mix(h01, h11, fx);
    float heightGray = mix(h0, h1, fy);

    // scale to the max height
    return (heightGray / 255.0f) * (float)maxHeight;
}

// Single-pass kernel: generate voxels with scaling/interpolation support
__kernel void generateVoxels(
    __global uchar4* input,
    __global float3* voxelPositions,
    int width,              // Image width
    int height,             // Image height
    int terrainWidth,       // Voxel terrain width
    int terrainHeight,      // Voxel terrain height
    float voxelSize,
    int maxHeight)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    
    // Now we iterate over terrain dimensions, not image dimensions
    if (x >= terrainWidth || y >= terrainHeight) return;
    
    // Calculate UV coordinates in image space
    float u = ((float)x / (float)(terrainWidth - 1)) * (float)(width - 1);
    float v = ((float)y / (float)(terrainHeight - 1)) * (float)(height - 1);
    
    // Sample height using bilinear interpolation
    float heightValue = sampleHeight(input, width, height, u, v, maxHeight);
    int heightVoxels = (int)round(heightValue);
    
    // Clamp to valid range
    heightVoxels = clamp(heightVoxels, 0, maxHeight);
    
    // Sample neighbor heights for filling
    int minNeighborHeight = heightVoxels;
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            
            int nx = x + dx;
            int ny = y + dy;
            
            if (nx < 0 || nx >= terrainWidth || ny < 0 || ny >= terrainHeight) continue;
            
            // Sample neighbor position in image space
            float nu = ((float)nx / (float)(terrainWidth - 1)) * (float)(width - 1);
            float nv = ((float)ny / (float)(terrainHeight - 1)) * (float)(height - 1);
            
            float neighborHeightValue = sampleHeight(input, width, height, nu, nv, maxHeight);
            int neighborHeight = (int)round(neighborHeightValue);
            neighborHeight = clamp(neighborHeight, 0, maxHeight);
            
            if (neighborHeight < minNeighborHeight) {
                minNeighborHeight = neighborHeight;
            }
        }
    }
    
    // Calculate output index based on terrain dimensions
    int idx = y * terrainWidth + x;
    int outputBase = idx * maxHeight;
    
    float worldX = (float)x * voxelSize;
    float worldZ = (float)y * voxelSize;
    
    // Generate voxels from minNeighborHeight to heightVoxels
    int writeOffset = 0;
    for (int h = minNeighborHeight; h <= heightVoxels && writeOffset < maxHeight; h++) {
        float worldY = (float)h * voxelSize;
        voxelPositions[outputBase + writeOffset] = (float3)(worldX, worldY, worldZ);
        writeOffset++;
    }
    
    // Mark remaining slots as invalid
    for (int i = writeOffset; i < maxHeight; i++) {
        voxelPositions[outputBase + i] = (float3)(NAN, NAN, NAN);
    }
}
    )";
}

MStatus HeightmapComputeShader::createKernels()
{
    const char* kernelSource = getKernelSource();

    fGenerateKernel = MOpenCLInfo::getOpenCLKernelFromString(
        kernelSource,
        "HeightmapVoxelProgram",
        "generateVoxels"
    );

    if (fGenerateKernel.get() == nullptr) {
        MGlobal::displayError("Failed to compile generateVoxels kernel");
        return MS::kFailure;
    }

    return MS::kSuccess;
}

MStatus HeightmapComputeShader::initialize()
{
    if (fInitialized) {
        MGlobal::displayWarning("HeightmapComputeShader already initialized");
        return MS::kSuccess;
    }

    fContext = MOpenCLInfo::getOpenCLContext();
    fQueue = MOpenCLInfo::getMayaDefaultOpenCLCommandQueue();

    if (!fContext || !fQueue) {
        MGlobal::displayError("Failed to get OpenCL context or queue");
        return MS::kFailure;
    }

    MStatus status = createKernels();
    if (status != MS::kSuccess) {
        return status;
    }

    fInitialized = true;
    return MS::kSuccess;
}

MStatus HeightmapComputeShader::generateVoxelsFromHeightmap(
    const MString& filepath,
    std::vector<MVector>& outVoxelPositions,
    unsigned int& terrainWidth,
    unsigned int& terrainHeight,
    float voxelSize,
    unsigned int maxHeight)
{
    if (!fInitialized) {
        MGlobal::displayError("HeightmapComputeShader not initialized. Call initialize() first.");
        return MS::kFailure;
    }

    if (voxelSize <= 0.0f) {
        MGlobal::displayError("Voxel size must be greater than zero");
        return MS::kFailure;
    }

    if (terrainWidth < 1 || terrainHeight < 1) {
        MGlobal::displayError("Terrain size must be atleast 1x1");
        return MS::kFailure;
    }

    // Load image
    MImage image;
    MStatus status = image.readFromFile(filepath);
    if (status != MS::kSuccess) {
        MGlobal::displayError("Failed to load image: " + filepath);
        return status;
    }

    unsigned int width, height;
    image.getSize(width, height);

    if (width == 0 || height == 0) {
        MGlobal::displayError("Invalid image dimensions");
        return MS::kFailure;
    }

    unsigned char* pixels = image.pixels();
    if (!pixels) {
        MGlobal::displayError("Failed to get image pixel data");
        return MS::kFailure;
    }

    // Find maximum grayscale value to optimize buffer size
    static const size_t BYTES_PER_PIXEL = 4; // RGBA
    size_t imagePixelCount = width * height;
    unsigned char maxGray = 0;

    for (size_t i = 0; i < imagePixelCount * BYTES_PER_PIXEL; i += BYTES_PER_PIXEL) {
        unsigned char gray = (pixels[i] + pixels[i + 1] + pixels[i + 2]) / 3;
        maxGray = std::max(maxGray, gray);
    }

    if (maxGray == 0) {
        MGlobal::displayWarning("Image is completely black, no voxels to generate");
        outVoxelPositions.clear();
        return MS::kSuccess;
    }

    MGlobal::displayInfo(MString("Max height: ") + maxHeight);

    size_t imageSize = imagePixelCount * BYTES_PER_PIXEL;
    cl_int err;

    // Create input buffer
    MAutoCLMem inputBuffer;
    cl_mem clInputBuffer = clCreateBuffer(fContext,
        CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
        imageSize, pixels, &err);
    if (err != CL_SUCCESS) {
        MGlobal::displayError("Failed to create input buffer");
        MOpenCLInfo::checkCLErrorStatus(err);
        return MS::kFailure;
    }
    inputBuffer.attach(clInputBuffer);

    // Create output buffer with fixed stride per terrain voxel
    size_t terrainPixelCount = terrainWidth * terrainHeight;
    size_t bufferSize = terrainPixelCount * maxHeight;
    
    MGlobal::displayInfo(MString("Allocating buffer for: ") + (bufferSize) + " voxel slots");
    
    MAutoCLMem voxelPositionsBuffer;
    cl_mem clVoxelPositions = clCreateBuffer(fContext,
        CL_MEM_WRITE_ONLY,
        bufferSize * sizeof(cl_float3), NULL, &err);
    if (err != CL_SUCCESS) {
        MGlobal::displayError("Failed to create voxel positions buffer");
        MOpenCLInfo::checkCLErrorStatus(err);
        return MS::kFailure;
    }
    voxelPositionsBuffer.attach(clVoxelPositions);

    // Execute single-pass kernel
    cl_kernel generateKernel = fGenerateKernel.get();
    clSetKernelArg(generateKernel, 0, sizeof(cl_mem), &clInputBuffer);
    clSetKernelArg(generateKernel, 1, sizeof(cl_mem), &clVoxelPositions);
    clSetKernelArg(generateKernel, 2, sizeof(int), &width);
    clSetKernelArg(generateKernel, 3, sizeof(int), &height);
    clSetKernelArg(generateKernel, 4, sizeof(int), &terrainWidth);
    clSetKernelArg(generateKernel, 5, sizeof(int), &terrainHeight);
    clSetKernelArg(generateKernel, 6, sizeof(float), &voxelSize);
    clSetKernelArg(generateKernel, 7, sizeof(int), &maxHeight);

    size_t globalWorkSize[2] = { terrainWidth, terrainHeight };
    err = clEnqueueNDRangeKernel(fQueue, generateKernel, 2, NULL,
        globalWorkSize, NULL, 0, NULL, NULL);
    if (err != CL_SUCCESS) {
        MGlobal::displayError("Failed to enqueue generateVoxels kernel");
        MOpenCLInfo::checkCLErrorStatus(err);
        return MS::kFailure;
    }
    clFinish(fQueue);

    // Read back voxel positions
    std::vector<cl_float3> clVoxelPositionsData(bufferSize);
    err = clEnqueueReadBuffer(fQueue, clVoxelPositions, CL_TRUE, 0,
        bufferSize * sizeof(cl_float3), clVoxelPositionsData.data(), 0, NULL, NULL);
    if (err != CL_SUCCESS) {
        MGlobal::displayError("Failed to read voxel positions");
        MOpenCLInfo::checkCLErrorStatus(err);
        return MS::kFailure;
    }

    // CPU post-processing
    outVoxelPositions.clear();

    // Estimate the voxel depth at a point is on average less than 3
    outVoxelPositions.reserve(terrainPixelCount * 3);
    for (size_t i = 0; i < bufferSize; i++) {
        const cl_float3& pos = clVoxelPositionsData[i];
        // Check if valid voxel
        if (!std::isnan(pos.s[0])) {
            outVoxelPositions.push_back(MVector(pos.s[0], pos.s[1], pos.s[2]));
        }
    }

    MGlobal::displayInfo(MString("Generated ") + (int)outVoxelPositions.size() + " voxels");

    return MS::kSuccess;
}

MStatus HeightmapComputeShader::generateVoxelsFromHeightmap(
    const MString& filepath,
    std::vector<MVector>& outVoxelPositions,
    unsigned int& outWidth,
    unsigned int& outHeight,
    unsigned int& terrainWidth,
    unsigned int& terrainHeight,
    float voxelSize,
    unsigned int maxHeight)
{
    if (!fInitialized) {
        MGlobal::displayError("HeightmapComputeShader not initialized. Call initialize() first.");
        return MS::kFailure;
    }

    MImage image;
    MStatus status = image.readFromFile(filepath);
    if (status != MS::kSuccess) {
        MGlobal::displayError("Failed to load image: " + filepath);
        return status;
    }

    image.getSize(outWidth, outHeight);
    return generateVoxelsFromHeightmap(filepath, 
        outVoxelPositions, terrainWidth, terrainHeight, voxelSize, maxHeight);
}

void HeightmapComputeShader::cleanup()
{
    if (fGenerateKernel.get()) {
        MOpenCLInfo::releaseOpenCLKernel(fGenerateKernel);
    }

    fContext = nullptr;
    fQueue = nullptr;
    fInitialized = false;
}