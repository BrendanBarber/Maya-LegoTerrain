#pragma once

#include <vector>
#include <maya/MGlobal.h>
#include <maya/MPxCommand.h>
#include <maya/MSyntax.h>

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

	std::unique_ptr<std::vector<float>> heightData;
	std::unique_ptr<std::vector<MVector>> voxelPositions;

	MObject particleSystemObj;
	MObject instancerObj;

	double brickScale;
	int imageWidth;
	int imageHeight;
	MString outputName;

	MStatus loadHeightmap(const MString& filepath, std::vector<float>& heightData,
		int& width, int& height);

	MStatus voxelizeHeightmap(const std::vector<float>& heightData, int width, int height,
		std::vector<MVector>& outVoxelPositions);

	MStatus createParticleSystem(const std::vector<MVector>& voxelPositions);
};