# Voxel World Generator for Unreal Engine 5

This project is a procedural voxel world generation system designed for integration into the Unreal Engine 5 game engine. It provides tools and components for creating dynamic, interactive, and visually appealing voxel-based environments.

## Features

*   **Procedural Terrain Generation:** Utilizes multi-layered noise functions (Perlin, Simplex, etc.) to generate diverse landscapes with realistic features such as mountains, valleys, and varying biomes.
*   **Dynamic Biome Creation:** Supports the creation and configuration of different biomes with specific characteristics (e.g., forest, desert, snow) based on temperature, humidity, and other environmental parameters.
*   **Chunk-Based World:** Organizes the world into manageable chunks for efficient memory management and dynamic loading/unloading.
*   **Multiple Meshing Algorithms:** Offers different methods for generating polygonal meshes from voxel data, including a basic "greedy meshing" implementation for optimized rendering performance.
*   **Destructible/Constructible Environment:** Allows players to interact with the environment by destroying and building voxel blocks, enabling dynamic world modification.
*   **Customizable Parameters:** Exposes a wide range of settings through Unreal Engine Data Assets (Data Tables, Curve Assets, Noise Presets) to allow for easy customization of world generation without requiring code changes.

## Technologies Used

*   **Unreal Engine 5:** Provides the core framework, rendering capabilities, and asset management.
*   **C++:** Used for implementing the core procedural generation algorithms and performance-critical components.
*   **FastNoiseLite (or similar):** A lightweight noise generation library for creating the terrain's heightmap and other environmental features.
*   **UProceduralMeshComponent:** An Unreal Engine component for dynamically creating and updating mesh geometry at runtime.

## Core Concepts

1.  **World Management (AChunkWorld):** The central actor responsible for managing the overall world, including chunk loading/unloading, and orchestrating the terrain generation.
2.  **Terrain Generation (UTerrainGenerator):** A component that handles the procedural generation of terrain data, biome selection, and vegetation placement.
3.  **Chunks (AChunkBase):** Individual 3D sections of the world, each represented by an actor that manages its voxel data and mesh.
4.  **Noise and Biomes (Data Assets):** Configuration assets (DataTables, Noise Settings) drive procedural variation and allow customization without code modifications.

## How It Works

The system creates voxel worlds by using a multi-stage process:

1.  **Initialization:** At startup, `AChunkWorld` reads parameters from the `UWorldGameInstance`.
2.  **Chunk Management:** The `AChunkWorld::Tick` function keeps track of the player's location and manages the active chunks in the world, loading and unloading them as needed.
3.  **Data Generation:** The `UTerrainGenerator::GenerateColumnData` function is responsible for creating the procedural height data, biomes and vegetation, based on global coordinates and using a combination of noise functions.
4.  **Meshing:** Each chunk then builds its mesh using `UProceduralMeshComponent`, which the system is rendering for.
5.  **Asynchronous Meshing:** Mesh creation operations are performed in a separate thread to prevent the main game thread from freezing during the intense computational process.
6.  **Dynamic Interaction:** Players can build or destroy structures with code modifying block structure and creating and destroying blocks.

## Setup Instructions

1.  Ensure you have Unreal Engine 5 installed. The project was developed and tested on version [Specify Unreal Engine Version].
2.  Clone or download this repository to your local machine.
3.  Open the `.uproject` file within the Unreal Engine 5 editor.
4.  Unreal Engine may prompt you to rebuild the project's C++ modules. Click "Yes" to rebuild.

## Usage

After successful project setup, you can:

1.  Place an `AChunkWorld` actor into the scene.
2.  Adjust the `DrawDistance`, `Seed`, and other settings on the `AChunkWorld` actor in the Details panel.
3.  Modify the `FBlockSettings` and `FBiomeSettings` Data Tables to change block types, materials, or biome distributions.
4.  Edit the `UNoiseGenerationPreset` Data Asset to change how noise is generated.

## Code Overview

*   **`AChunkWorld.h/cpp`**: Manages the overall world, chunk lifecycle, and initializes the terrain generation.
*   **`UTerrainGenerator.h/cpp`**: Implements the terrain generation and biome assignment logic.
*   **`AChunkBase.h/cpp`**: Abstract base class for all chunk actors.
*   **`ADefaultChunk.h/cpp`**: An example implementation of chunk meshing.
*   **`AGreedyChunk.h/cpp`**: An optimized chunk implementation that has a greedy algorithm.
*   **`UFoliageGenerator.h/cpp`**: Handles placement of plant and forest life over all loaded in chunks.
*   **`FChunkColumn.h`**: Structure containing per-column data in a chunk.
*   **`FChunkMeshData.h`**: Structure used to store data for visualization by the procedural mesh component.

## References
* https://www.alanzucconi.com/2022/06/05/minecraft-world-generation/
* https://minecraft.wiki/w/World_generation
* https://www.youtube.com/watch?v=CSa5O6knuwI

## Screenshots

*   ![](https://i.imgur.com/mfSSKlV.jpeg)
*   ![](https://i.imgur.com/9whCssb.jpeg)
*   ![](https://i.imgur.com/uybEOEY.png)
*   ![Naive meshing](https://i.imgur.com/t5PcGS9.jpeg)
*   ![Greedy meshing](https://i.imgur.com/v8EesGR.jpeg)
