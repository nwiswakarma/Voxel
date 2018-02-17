// 

#pragma once

#include "CoreMinimal.h"
#include "Octree.h"
#include "VoxelBox.h"

class FVoxelChunkNode;
class FVoxelRender;
class UVoxelInvokerComponent;
class UVoxelProceduralMeshComponent;

/**
 * Create the octree for rendering and spawn VoxelChunks
 */
class FChunkOctree : public FOctree
{
public:

	FVoxelRender* const Render;

	FChunkOctree(FVoxelRender* Render, FIntVector Position, uint8 Depth, uint64 Id, uint64 MeshId);
	~FChunkOctree();

	/**
	 * Unload VoxelChunk if created and recursively delete childs
	 */
	void Destroy();

	/**
	 * Registers chunk for update
	 * @param	bAsync			Whether to update asynchorously
	 * @param	bRecursive		Whether to update recurcively
	 */
	void UpdateChunk(bool bAsync, bool bRecursive);

	/**
	 * Create/Update the octree for the new position
	 */
	void UpdateLOD();

	/**
	 * Get a weak pointer to the leaf chunk at PointPosition. Weak pointers allow to check that the object they are pointing to is valid
	 * @param	PointPosition	Position in voxel space. Must be contained in this octree
	 * @return	Weak pointer to leaf chunk at PointPosition
	 */
	FChunkOctree* GetLeaf(FIntVector PointPosition);

	/**
	 * Return chunk node
	 * @return	VoxelChunk; can be nullptr
	 */
	FORCEINLINE FVoxelChunkNode* GetVoxelChunk() const;

	FORCEINLINE void AssignMeshId();
	FORCEINLINE void ResetMeshId();
	FORCEINLINE uint64 GetMeshId() const;

	/**
	* Get direct child at position. Must not be leaf
	* @param	PointPosition	Position in voxel space. Must be contained in this octree
	* @return	Direct child in which PointPosition is contained
	*/
	FChunkOctree* GetChild(FIntVector PointPosition);

	void GetLeafsOverlappingBox(FVoxelBox Box, std::forward_list<FChunkOctree*>& Octrees);

private:

	/**
	 *  Childs of this octree in the following order:
     *
	 *  bottom      top
	 *  -----> y
	 *  | 0 | 2    4 | 6
	 *  v 1 | 3    5 | 7
	 *  x
	 */
	TArray<FChunkOctree*, TFixedAllocator<8>> Childs;

	// Whether chunk contains a mesh chunk node
	bool bHasChunk;

    // Assigned mesh id
    uint64 MeshId;

	// Pointer to the mesh chunk node
	FVoxelChunkNode* VoxelChunk;

	/**
	 * Create the VoxelChunk
	 */
	void Load();

	/**
	 * Unload the VoxelChunk
	 */
	void Unload();

	/**
	 * Create childs of this octree
	 */
	void CreateChilds();

	/**
	 * Destroy childs (with their chunks)
	 */
	void DestroyChilds();
};
