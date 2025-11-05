#include "VoxelizeTerrainCmd.h"
#include "HeightmapComputeShader.h"
#include <maya/MImage.h>
#include <maya/MArgDatabase.h>
#include <maya/MVectorArray.h>
#include <maya/MSelectionList.h>
#include <maya/MDagPath.h>
#include <maya/MFnInstancer.h>
#include <maya/MDGModifier.h>
#include <maya/MPlug.h>
#include <maya/MPointArray.h>
#include <chrono>
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
	MStatus status;
	MDGModifier dgMod;

	if (!m_particleSystemObj.isNull()) {
		dgMod.deleteNode(m_particleSystemObj);
	}
	if (!m_instancerObj.isNull()) {
		dgMod.deleteNode(m_instancerObj);
	}

	return dgMod.doIt();
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
		unsigned char fileSignature[8] = {0, 0, 0, 0, 0, 0, 0, 0};

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
	MStatus status;
	
	// Load the heightmap to get voxel positions
	status = loadHeightmap(m_heightmapPath, m_voxelPositions);
	CHECK_MSTATUS_AND_RETURN_IT(status);

	// Use the voxel positions to create a particle system
	status = createParticleSystem(m_voxelPositions);
	CHECK_MSTATUS_AND_RETURN_IT(status);

	return MS::kSuccess;
}

MStatus VoxelizeTerrainCmd::createParticleSystem(const std::vector<MVector>& voxelPositions)
{
	MStatus status;

	// Disable evaluation manager before creating particles
	MGlobal::executeCommand("evaluationManager -mode \"off\"");

	// Create particle object
	MString particleName = "voxelParticles_" + m_outputName;
	MGlobal::executeCommand("particle -name " + particleName);

	MGlobal::executeCommand("select -clear");

	// Get the particle shape node
	MSelectionList selList;
	MString particleShapeName = particleName + "Shape";
	status = selList.add(particleShapeName);
	if (status != MS::kSuccess) {
		MGlobal::displayError("Particle shape was not found: " + particleShapeName);
		return status;
	}

	status = selList.getDependNode(0, m_particleSystemObj);
	CHECK_MSTATUS_AND_RETURN_IT(status);

	MFnParticleSystem particleFn(m_particleSystemObj, &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);

	int oldCount = particleFn.count();
	int newCount = voxelPositions.size();
	int totalCount = oldCount + newCount;

	// Set new count first
	particleFn.setCount(totalCount);

	// Get attribute arrays
	MVectorArray positions;
	MVectorArray velocities;
	particleFn.position(positions);
	particleFn.velocity(velocities);

	// Resize
	positions.setLength(totalCount);
	velocities.setLength(totalCount);

	// Bulk copy new data
	memcpy(&positions[oldCount], voxelPositions.data(), newCount * sizeof(MVector));

	// Zero out new velocities
	memset(&velocities[oldCount], 0, newCount * sizeof(MVector));

	// Apply back
	particleFn.setPerParticleAttribute("position", positions);
	particleFn.setPerParticleAttribute("velocity", velocities);

	particleFn.saveInitialState();

	MString cubeName = "voxelCube_" + m_outputName;
	MGlobal::executeCommand("polyCube -name " + cubeName, status);
	CHECK_MSTATUS_AND_RETURN_IT(status);

	MString instancerName = "voxelInstancer_" + m_outputName;
	MGlobal::executeCommand("particleInstancer -name " + instancerName +
		" -addObject -object " + cubeName + " " + particleName, status);
	CHECK_MSTATUS_AND_RETURN_IT(status);

	status = selList.clear();
	status = selList.add(instancerName);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	status = selList.getDependNode(0, m_instancerObj);
	CHECK_MSTATUS_AND_RETURN_IT(status);

	MGlobal::executeCommand("hide " + cubeName);

	return MS::kSuccess;
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