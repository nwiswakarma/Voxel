// Copyright 2017 Phyronnaz

#pragma once

#include "CoreMinimal.h"
#include "VoxelProceduralMeshTypes.h"

class FVoxelRender;
class FChunkOctree;
class FVoxelPolygonizer;
class FAsyncPolygonizerTask;

/**
 * Voxel Chunk actor class
 */
class FVoxelChunkNode
{
public:

	FVoxelChunkNode(FChunkOctree* NewOctree);
	~FVoxelChunkNode();

	/**
	 * Destroy chunk node
	 */
	void Destroy();

	/**
	 * Update this for terrain changes
	 * @param	bAsync
	 */
	bool Update(bool bAsync);

	/**
     * Copy Task section to PrimaryMesh section
     */
	void OnMeshComplete(FVoxelProcMeshSection& InSection);

    /**
     * Apply generated mesh section to a node mesh
     */
	void ApplyMesh();

    /**
     * Apply node offset to mesh geometry
     */
	FORCEINLINE void ApplyMeshOffset();

    FORCEINLINE bool IsValid() const
    {
        return Render != nullptr;
    }

	FORCEINLINE bool CanCreateSection() const
    {
        return !bAbandonBuilder;
    }

private:
    friend class FAsyncPolygonizerTask;

    // Node data
    uint64 const MeshId;
    uint8 const Depth;
    FIntVector const Offset;
	FVoxelProcMeshSection Section;

	// ChunkHasHigherRes[TransitionDirection] if Depth != 0
	TArray<bool, TFixedAllocator<6>> ChunkHasHigherRes;

	// Mesh builder tools
	FThreadSafeBool bAbandonBuilder;
	FAsyncTask<FAsyncPolygonizerTask>* MeshBuilderTask;

    // Render data objects
	FChunkOctree* const CurrentOctree;
	FVoxelRender* const Render;

	void EnsureTaskCompletion(bool bCancel = false);

	TSharedPtr<FVoxelPolygonizer> CreatePolygonizer();
};
