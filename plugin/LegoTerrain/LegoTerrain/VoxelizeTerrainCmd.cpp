#include "VoxelizeTerrainCmd.h"
#include "HeightmapComputeShader.h"
#include <maya/MImage.h>
#include <GL/GLU.h>

const char* VoxelizeTerrainCmd::commandName = "voxelizeTerrain";

const char* VoxelizeTerrainCmd::heightMapFlag = "-h";
const char* VoxelizeTerrainCmd::heightMapFlagLong = "-heightMapPath";
const char* VoxelizeTerrainCmd::brickScaleFlag = "-s";
const char* VoxelizeTerrainCmd::brickScaleFlagLong = "-brickScale";
const char* VoxelizeTerrainCmd::terrainDimensionsFlag = "-d";
const char* VoxelizeTerrainCmd::terrainDimensionsFlagLong = "-terrainDimensions";
const char* VoxelizeTerrainCmd::outputNameFlag = "-o";
const char* VoxelizeTerrainCmd::outputNameFlagLong = "-outputName";

VoxelizeTerrainCmd::VoxelizeTerrainCmd()
{
	VoxelizeTerrainCmd::m_voxelPositions = nullptr;

	//VoxelizeTerrainCmd::particleSystemObj is null
	//VoxelizeTerrainCmd::instancerObj is null

	VoxelizeTerrainCmd::m_brickScale = 1.0;
	VoxelizeTerrainCmd::m_imageWidth = 512;
	VoxelizeTerrainCmd::m_imageHeight = 512;
	
	//VoxelizeTerrainCmd::outputName is null
}

VoxelizeTerrainCmd::~VoxelizeTerrainCmd() 
{

}

void* VoxelizeTerrainCmd::creator()
{
	return new VoxelizeTerrainCmd();
}

MSyntax VoxelizeTerrainCmd::newSyntax()
{
	MSyntax syntax;
	syntax.addFlag(heightMapFlag, heightMapFlagLong, MSyntax::kString);
	syntax.addFlag(brickScaleFlag, brickScaleFlagLong, MSyntax::kDouble);
	syntax.addFlag(terrainDimensionsFlag, terrainDimensionsFlagLong, MSyntax::kLong, MSyntax::kLong);
	syntax.addFlag(outputNameFlag, outputNameFlagLong, MSyntax::kString);
	return syntax;
}

MStatus VoxelizeTerrainCmd::doIt(const MArgList& args)
{

}

MStatus VoxelizeTerrainCmd::redoIt()
{

}

MStatus VoxelizeTerrainCmd::undoIt()
{

}

bool VoxelizeTerrainCmd::isUndoable() const {
	return true;
}

MStatus VoxelizeTerrainCmd::loadHeightmap(const MString& filepath, std::vector<MVector>& outVoxelPositions)
{
	HeightmapComputeShader shader;
	MStatus status = shader.initialize();

	if (status != MS::kSuccess) {
		MGlobal::displayError("Failed to initialize HeightmapComputeShader");
		return status;
	}

	status = shader.generateVoxelsFromHeightmap(
		filepath,
		outVoxelPositions,
		m_imageWidth,
		m_imageHeight,
		m_brickScale
	);

	if (status == MS::kSuccess) {
		MGlobal::displayInfo(MString("Generated ") + (int)outVoxelPositions.size() + " voxels");
		MGlobal::displayInfo(MString("Image dimensions: ") + m_imageWidth + "x" + m_imageHeight);
	}

	shader.cleanup();
	return status;
}