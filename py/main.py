import maya.cmds as cmds
import maya.mel as mel

class TerrainGenerateShelf:

    def __init__(self, name="TerrainGenerate"):
        self.name = name
        self.label_background = (0, 0, 0, 0)
        self.label_color = (0.9, 0.9, 0.9)

        self._cleanOldShelf()
        cmds.setParent(self.name)
        self.build()

    def build(self):
        self.addButton(
            label="Generate",
            icon="TypeDefaultMaterial.png",
            command="""
import maya.cmds as cmds
from PySide6 import QtWidgets, QtCore
import os


class VoxelizeTerrainUI(QtWidgets.QDialog):
    def __init__(self, parent=None):
        super(VoxelizeTerrainUI, self).__init__(parent)

        self.setWindowTitle("Voxelize Terrain")
        self.setMinimumSize(400, 300)

        self.create_ui()

    def create_ui(self):
        main_layout = QtWidgets.QVBoxLayout(self)

        # Heightmap path
        heightmap_layout = QtWidgets.QHBoxLayout()
        heightmap_label = QtWidgets.QLabel("Heightmap Path:")
        self.heightmap_field = QtWidgets.QLineEdit()
        self.heightmap_field.setPlaceholderText("Path to PNG heightmap file")
        self.browse_button = QtWidgets.QPushButton("Browse...")
        self.browse_button.clicked.connect(self.browse_heightmap)
        heightmap_layout.addWidget(heightmap_label)
        heightmap_layout.addWidget(self.heightmap_field)
        heightmap_layout.addWidget(self.browse_button)
        main_layout.addLayout(heightmap_layout)

        # Brick scale
        brick_scale_layout = QtWidgets.QHBoxLayout()
        brick_scale_label = QtWidgets.QLabel("Brick Scale:")
        self.brick_scale_spin = QtWidgets.QDoubleSpinBox()
        self.brick_scale_spin.setRange(0.01, 1000.0)
        self.brick_scale_spin.setValue(1.0)
        self.brick_scale_spin.setDecimals(2)
        brick_scale_layout.addWidget(brick_scale_label)
        brick_scale_layout.addWidget(self.brick_scale_spin)
        main_layout.addLayout(brick_scale_layout)

        # Terrain width
        terrain_width_layout = QtWidgets.QHBoxLayout()
        terrain_width_label = QtWidgets.QLabel("Terrain Width:")
        self.terrain_width_spin = QtWidgets.QSpinBox()
        self.terrain_width_spin.setRange(1, 10000)
        self.terrain_width_spin.setValue(512)
        terrain_width_layout.addWidget(terrain_width_label)
        terrain_width_layout.addWidget(self.terrain_width_spin)
        main_layout.addLayout(terrain_width_layout)

        # Terrain height
        terrain_height_layout = QtWidgets.QHBoxLayout()
        terrain_height_label = QtWidgets.QLabel("Terrain Height:")
        self.terrain_height_spin = QtWidgets.QSpinBox()
        self.terrain_height_spin.setRange(1, 10000)
        self.terrain_height_spin.setValue(512)
        terrain_height_layout.addWidget(terrain_height_label)
        terrain_height_layout.addWidget(self.terrain_height_spin)
        main_layout.addLayout(terrain_height_layout)

        # Max height
        max_height_layout = QtWidgets.QHBoxLayout()
        max_height_label = QtWidgets.QLabel("Max Height:")
        self.max_height_spin = QtWidgets.QSpinBox()
        self.max_height_spin.setRange(0, 256)
        self.max_height_spin.setValue(256)
        max_height_layout.addWidget(max_height_label)
        max_height_layout.addWidget(self.max_height_spin)
        main_layout.addLayout(max_height_layout)

        # Output name
        output_name_layout = QtWidgets.QHBoxLayout()
        output_name_label = QtWidgets.QLabel("Output Name:")
        self.output_name_field = QtWidgets.QLineEdit()
        self.output_name_field.setText("terrain")
        self.output_name_field.setPlaceholderText("Name for output objects")
        output_name_layout.addWidget(output_name_label)
        output_name_layout.addWidget(self.output_name_field)
        main_layout.addLayout(output_name_layout)

        # Generate button
        self.run_button = QtWidgets.QPushButton("Generate Terrain")
        self.run_button.clicked.connect(self.on_button_clicked)
        main_layout.addWidget(self.run_button)

    def browse_heightmap(self):
        file_path, _ = QtWidgets.QFileDialog.getOpenFileName(
            self,
            "Select Heightmap",
            "",
            "PNG Images (*.png);;All Files (*)"
        )
        if file_path:
            self.heightmap_field.setText(file_path)

    def on_button_clicked(self):
        # Get heightmap path
        heightmap_path = self.heightmap_field.text()

        if not heightmap_path:
            QtWidgets.QMessageBox.warning(self, "No Heightmap", "Please select a heightmap file.")
            return

        if not os.path.exists(heightmap_path):
            QtWidgets.QMessageBox.warning(self, "File Not Found", "The specified heightmap file does not exist.")
            return

        # Get parameters
        brick_scale = self.brick_scale_spin.value()
        terrain_width = self.terrain_width_spin.value()
        terrain_height = self.terrain_height_spin.value()
        max_height = self.max_height_spin.value()
        output_name = self.output_name_field.text()

        if not output_name:
            QtWidgets.QMessageBox.warning(self, "No Output Name", "Please enter an output name.")
            return

        try:
            cmds.voxelizeTerrain(
                heightMapPath=heightmap_path,
                brickScale=brick_scale,
                terrainDimensions=(terrain_width, terrain_height),
                maxHeight=max_height,
                outputName=output_name
            )
            print(f"Terrain generated: {output_name}")
            QtWidgets.QMessageBox.information(self, "Success", f"Terrain '{output_name}' generated successfully!")
        except Exception as e:
            QtWidgets.QMessageBox.critical(self, "Error", f"Failed to generate terrain:\\n{str(e)}")


def show_ui():
    global voxelize_ui

    try:
        voxelize_ui.close()
        voxelize_ui.deleteLater()
    except:
        pass

    voxelize_ui = VoxelizeTerrainUI()
    voxelize_ui.show()


show_ui()

            """
        )

    def addButton(self, label, icon="TypeDefaultMaterial.png", command="", double_command=""):
        cmds.setParent(self.name)

        cmds.shelfButton(
            width=37,
            height=37,
            image=icon,
            label=label,
            command=command,
            doubleClickCommand=double_command,
            annotation=f"",
            sourceType="python"
        )

    def _cleanOldShelf(self):
        if cmds.shelfLayout(self.name, exists=True):
            if cmds.shelfLayout(self.name, query=True, childArray=True):
                for e in cmds.shelfLayout(self.name, query=True, childArray=True):
                    cmds.deleteUI(e)
        else:
            cmds.shelfLayout(self.name, parent="ShelfLayout")

TerrainGenerateShelf()