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
// Single-pass kernel: generate voxels with fixed stride per pixel
__kernel void generateVoxels(
    __global uchar4* input,
    __global float3* voxelPositions,
    int width,
    int height,
    float voxelSize,
    int maxHeight)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    
    if (x >= width || y >= height) return;
    
    int idx = y * width + x;
    uchar4 pixel = input[idx];
    
    // Convert to grayscale (0-255 range)
    float gray = (pixel.x + pixel.y + pixel.z) / 3.0f;
    int heightVoxels = (int)(gray);
    
    // Find minimum neighbor height to fill down to
    int minNeighborHeight = heightVoxels;
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            
            int nx = x + dx;
            int ny = y + dy;
            
            if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
            
            int nidx = ny * width + nx;
            uchar4 neighborPixel = input[nidx];
            float neighborGray = (neighborPixel.x + neighborPixel.y + neighborPixel.z) / 3.0f;
            int neighborHeight = (int)(neighborGray);
            
            if (neighborHeight < minNeighborHeight) {
                minNeighborHeight = neighborHeight;
            }
        }
    }
    
    // Calculate base offset for this pixel's voxel column
    int outputBase = idx * maxHeight;
    
    float worldX = (float)x * voxelSize;
    float worldZ = (float)y * voxelSize;
    
    // Generate voxels from minNeighborHeight to heightVoxels
    int writeOffset = 0;
    for (int h = minNeighborHeight; h < heightVoxels; h++) {
        float worldY = (float)h * voxelSize;
        voxelPositions[outputBase + writeOffset] = (float3)(worldX, worldY, worldZ);
        writeOffset++;
    }
    
    // Mark remaining slots as invalid using NaN
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
    float voxelSize)
{
    if (!fInitialized) {
        MGlobal::displayError("HeightmapComputeShader not initialized. Call initialize() first.");
        return MS::kFailure;
    }

    if (voxelSize <= 0.0f) {
        MGlobal::displayError("Voxel size must be greater than zero");
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
    size_t pixelCount = width * height;
    unsigned char maxGray = 0;

    for (size_t i = 0; i < pixelCount * BYTES_PER_PIXEL; i += BYTES_PER_PIXEL) {
        unsigned char gray = (pixels[i] + pixels[i + 1] + pixels[i + 2]) / 3;
        maxGray = std::max(maxGray, gray);
    }

    if (maxGray == 0) {
        MGlobal::displayWarning("Image is completely black, no voxels to generate");
        outVoxelPositions.clear();
        return MS::kSuccess;
    }

    int maxHeight = (int)maxGray + 1;

    MGlobal::displayInfo(MString("Max height detected: ") + maxHeight);
    MGlobal::displayInfo(MString("Allocating buffer for: ") + (pixelCount * maxHeight) + " voxel slots");

    size_t imageSize = pixelCount * BYTES_PER_PIXEL;
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

    // Create output buffer with fixed stride per pixel
    size_t bufferSize = pixelCount * maxHeight;
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
    clSetKernelArg(generateKernel, 4, sizeof(float), &voxelSize);
    clSetKernelArg(generateKernel, 5, sizeof(int), &maxHeight);

    size_t globalWorkSize[2] = { width, height };
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
    outVoxelPositions.reserve(pixelCount * 3);
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
    float voxelSize)
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
    return generateVoxelsFromHeightmap(filepath, outVoxelPositions, voxelSize);
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