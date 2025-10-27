#include <maya/MFnPlugin.h>
#include <maya/MGlobal.h>

#include "VoxelizeTerrainCmd.h"

MStatus initializePlugin(MObject obj)
{
	const char* pluginVendor = "Brendan Barber";
	const char* pluginVersion = "0.1";

	MFnPlugin fnPlugin(obj, pluginVendor, pluginVersion);

	fnPlugin.registerCommand(VoxelizeTerrainCmd::commandName, VoxelizeTerrainCmd::creator, VoxelizeTerrainCmd::newSyntax);

	MGlobal::displayInfo("Plugin has been initialized!");

	return (MS::kSuccess);
}

MStatus uninitializePlugin(MObject obj)
{
	MGlobal::displayInfo("Plugin has been uninitialized!");

	return (MS::kSuccess);
}