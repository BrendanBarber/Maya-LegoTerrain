# Maya-LegoTerrainGenerator

A Maya plugin for generating terrain in the style of lego bricks.

Currently working on:
    - Scale the heightmap output to be the input x,y dimension
    - PySide interface with
        - Heightmap path (and viewer?)
        - Brick Size
        - Dimensions
        - Max height
        - Output Name
        * Include info for generation time and then render time 
    - Lego brick model to be instanced

Possible Additions:
- Add colors
- Add support for LOD, with different instance types
- Add support for more brick types:
    - Stacked flat bricks to get more accurate and lego smooth terrain
    - Merge collections of bricks together into larger bricks
    - Use 1x1 bricks instead
