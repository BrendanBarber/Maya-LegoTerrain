#pragma once

#include <maya/MGlobal.h>
#include <maya/MStatus.h>
#include <maya/MString.h>
#include <maya/MVector.h>
#include <maya/MOpenCLAutoPtr.h>
#include <vector>

typedef struct _cl_context* cl_context;
typedef struct _cl_command_queue* cl_command_queue;

/**
 * @brief GPU-accelerated heightmap to voxel converter using OpenCL
 *
 * This class uses OpenCL compute shaders to efficiently convert a heightmap
 * image into a 3D voxel grid. It performs a two-pass algorithm:
 * 1. Count the number of voxels each pixel will generate
 * 2. Generate the actual voxel positions based on the counts
 */
class HeightmapComputeShader
{
public:
    HeightmapComputeShader();
    ~HeightmapComputeShader();

    MStatus initialize();

    MStatus generateVoxelsFromHeightmap(
        const MString& filepath,
        std::vector<MVector>& outVoxelPositions,
        float voxelSize = 1.0f);

    MStatus generateVoxelsFromHeightmap(
        const MString& filepath,
        std::vector<MVector>& outVoxelPositions,
        unsigned int& outWidth,
        unsigned int& outHeight,
        float voxelSize = 1.0f);

    void cleanup();
    bool isInitialized() const;

private:
    cl_context fContext;
    cl_command_queue fQueue;
    MAutoCLKernel fCountKernel;
    MAutoCLKernel fGenerateKernel;
    bool fInitialized;

    MStatus createKernels();

    static const char* getKernelSource();
};