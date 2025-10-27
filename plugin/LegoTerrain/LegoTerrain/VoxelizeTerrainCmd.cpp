#include "VoxelizeTerrainCmd.h"
#include "HeightmapComputeShader.h"
#include <maya/MImage.h>
#include <maya/MArgDatabase.h>
#include <fstream>

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

	//VoxelizeTerrainCmd::m_particleSystemObj is null
	//VoxelizeTerrainCmd::m_instancerObj is null

	VoxelizeTerrainCmd::m_heightmapPath = "";
	VoxelizeTerrainCmd::m_brickScale = 1.0;
	VoxelizeTerrainCmd::m_terrainWidth = 512;
	VoxelizeTerrainCmd::m_terrainHeight = 512;
	
	VoxelizeTerrainCmd::m_outputName = "terrain";
	VoxelizeTerrainCmd::m_hasValidData = false;
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
	MStatus status = parseArguments(args);
	if (!status) return status;

	return redoIt();
}

MStatus VoxelizeTerrainCmd::redoIt()
{
	if (!m_hasValidData) {
		return MS::kFailure;
	}

	return executeCommand();
}

MStatus VoxelizeTerrainCmd::undoIt()
{

}

bool VoxelizeTerrainCmd::isUndoable() const {
	return true;
}

MStatus VoxelizeTerrainCmd::parseArguments(const MArgList& args)
{
	MArgDatabase argData(newSyntax(), args);

	// Get heightmap path
	if (argData.isFlagSet(heightMapFlag)) {
		MString heightMapPath = argData.flagArgumentString(heightMapFlag, 0);
		
		if (heightMapPath.length() == 0) {
			MGlobal::displayError("Height map path is empty");
			return MS::kFailure;
		}

		std::ifstream file(heightMapPath.asChar(), std::ios::binary);
		if (!file.good()) {
			MGlobal::displayError("Height map file does not exist: " + heightMapPath);
			return MS::kFailure;
		}

		MString lowercasePath = heightMapPath.toLowerCase();
		if (!(lowercasePath.substring(lowercasePath.length() - 4, lowercasePath.length() - 1) == ".png")) {
			MGlobal::displayError("Height map file is not in PNG format: " + heightMapPath);
			file.close();
			return MS::kFailure;
		}

		unsigned char pngSignature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
		unsigned char fileSignature[8];

		file.read(reinterpret_cast<char*>(fileSignature), 8);
		file.close();

		if (file.gcount() < 8 || memcmp(fileSignature, pngSignature, 8) != 0) {
			MGlobal::displayError("Height map file is not in PNG format: " + heightMapPath);
			return MS::kFailure;
		}

		m_heightmapPath = heightMapPath;
	}

	// Get brick scale
	if (argData.isFlagSet(brickScaleFlag)) {
		float brickScale = static_cast<float>(argData.flagArgumentDouble(brickScaleFlag, 0));

		if (brickScale <= 0) {
			MGlobal::displayError("Brick scale must be a float greater than 0");
			return MS::kFailure;
		}

		m_brickScale = brickScale;
	}

	// Get terrain dimensions
	if (argData.isFlagSet(terrainDimensionsFlag)) {
		int terrainWidth = argData.flagArgumentInt(terrainDimensionsFlag, 0);
		int terrainHeight = argData.flagArgumentInt(terrainDimensionsFlag, 1);

		if (terrainWidth <= 0 || terrainHeight <= 0) {
			MGlobal::displayError("Terrain width and height must be a integer greater than 0");
			return MS::kFailure;
		}

		m_terrainWidth = terrainWidth;
		m_terrainHeight = terrainHeight;
	}

	// Get output name
	if (argData.isFlagSet(outputNameFlag)) {
		MString outputName = argData.flagArgumentString(outputNameFlag, 0);

		if (outputName.length() == 0) {
			MGlobal::displayError("You must specify an output name for the terrain");
			return MS::kFailure;
		}

		m_outputName = outputName;
	}

	m_hasValidData = true;
	return MS::kSuccess;
}

MStatus VoxelizeTerrainCmd::executeCommand() {
	
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