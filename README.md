# Maya-LegoTerrainGenerator

A Maya plugin for generating terrain in the style of lego bricks.

Implementation plan:
- Input: Heightmap
    - Convert heightmap to a grid of voxel positions, take into account slope on steep areas to avoid vertical gaps
        - To handle steep slopes, check y value of voxels in the xz directions, find the lowest voxel and keep adding below until filled


- Output: Particle Instancer
    - At each position instance a lego brick particle
        - set by x y z property
        - Include an unused color property

Possible Additions:
- Add colors
- Add support for LOD, with different instance types
- Add support for more brick types:
    - Stacked flat bricks to get more accurate and lego smooth terrain
    - Merge collections of bricks together into larger bricks
    - Use 1x1 bricks instead
