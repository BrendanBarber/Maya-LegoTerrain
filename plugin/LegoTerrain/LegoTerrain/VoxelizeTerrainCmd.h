#pragma once

#include <vector>
#include <maya/MGlobal.h>
#include <maya/MPxCommand.h>
#include <maya/MSyntax.h>
#include <maya/MFnParticleSystem.h>

class VoxelizeTerrainCmd : public MPxCommand
{
public:
	static const char* commandName;

	VoxelizeTerrainCmd();
	virtual ~VoxelizeTerrainCmd();


	virtual MStatus doIt(const MArgList& args) override;
	virtual MStatus redoIt() override;
	virtual MStatus undoIt() override;
	virtual bool isUndoable() const override;

	static void* creator();

	static MSyntax newSyntax();

private:
	static const char* heightMapFlag;
	static const char* heightMapFlagLong;
	static const char* brickScaleFlag;
	static const char* brickScaleFlagLong;
	static const char* terrainDimensionsFlag;
	static const char* terrainDimensionsFlagLong;
	static const char* outputNameFlag;
	static const char* outputNameFlagLong;

	std::vector<MVector> m_voxelPositions;

	MObject m_particleSystemObj;
	MObject m_instancerObj;

	MString m_heightmapPath;
	float m_brickScale;
	unsigned int m_terrainWidth;
	unsigned int m_terrainHeight;
	unsigned int m_imageWidth;
	unsigned int m_imageHeight;
	MString m_outputName;
	bool m_hasValidData;

	MStatus parseArguments(const MArgList& args);
	MStatus executeCommand();

	MStatus loadHeightmap(const MString& filepath, std::vector<MVector>& outVoxelPositions);
	MStatus createParticleSystem(const std::vector<MVector>& voxelPositions);
};