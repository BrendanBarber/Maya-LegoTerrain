#include "VoxelizeTerrainCmd.h"

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
	VoxelizeTerrainCmd::heightData = nullptr;
	VoxelizeTerrainCmd::voxelPositions = nullptr;

	//VoxelizeTerrainCmd::particleSystemObj is null
	//VoxelizeTerrainCmd::instancerObj is null

	VoxelizeTerrainCmd::brickScale = 1.0;
	VoxelizeTerrainCmd::imageWidth = 512;
	VoxelizeTerrainCmd::imageHeight = 512;
	
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

