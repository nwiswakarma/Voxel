// Copyright 2017 Phyronnaz

#pragma once

#include "CoreMinimal.h"
#include "VoxelBox.h"
#include "VoxelProceduralMeshTypes.h"
#include "VoxelThreadPool.h"
#include "Containers/Queue.h"
#include <list>

class AVoxelWorld;
class UVoxelProceduralMeshComponent;
class FVoxelData;
class FChunkOctree;
class FVoxelChunkNode;
class FVoxelCacheBuilder;
class FVoxelDBCacheWorker;
class FVoxelDBFetchData;
struct FVoxelDBCacheData;

/**
 *
 */
class FVoxelRender
{
    typedef FVoxelThreadPool FThreadPool;
    typedef FThreadPool::FSlotData FThreadPoolSlotData;

    // Instanced thread pool generated from IVoxel::GetThreadPoolInstance()
	TSharedPtr< FThreadPool > RenderThreadPool;

    // Instanced DB cache worker generated from IVoxel::GetDBCacheWorker()
	TSharedPtr< FVoxelDBCacheWorker > DBCacheWorker;

public:

	AVoxelWorld* const World;
	AActor* const ChunksParent;
	FVoxelData* const Data;

	FVoxelRender(AVoxelWorld* World, AActor* ChunksParent, FVoxelData* Data);
	~FVoxelRender();

	// MUST be called before delete
	void Destroy();

	void Load();
	void Unload();

	void Tick(float DeltaTime);

	void UpdateAll(bool bAsync);
	void UpdateChunk(FChunkOctree* Chunk, bool bAsync);
	void UpdateChunksAtPosition(FIntVector Position, bool bAsync);
	void UpdateChunksOverlappingBox(FVoxelBox Box, bool bAsync);

	FORCEINLINE void EnqueueMeshChunk(FVoxelChunkNode* Chunk);
	FORCEINLINE void EnqueueFinishedCacheBuilder(FVoxelCacheBuilder* Task);
	FORCEINLINE void EnqueueCacheData(TSharedPtr<FVoxelDBCacheData>& CacheData);

	FORCEINLINE bool HasMesh(uint64 MeshId) const;
	FORCEINLINE FChunkOctree* GetOctree();
	FChunkOctree* GetChunkOctreeAt(FIntVector Position) const;

	int GetDepthAt(FIntVector Position) const;
	void SetVisibleLOD(int32 NewVisibleLOD);

    // Add LOD invoker component
	void AddInvoker(TWeakObjectPtr<UVoxelInvokerComponent> Invoker);

	// Needed when ChunksParent != World
	FVector GetGlobalPosition(FIntVector LocalPosition);

    // Get render thread pool
    FORCEINLINE FQueuedThreadPool* const GetRenderThreadPool();

    // Get database cache worker
    FORCEINLINE void CreateDBCacheWorker();
    FORCEINLINE FVoxelDBCacheWorker& GetDBCacheWorker();

    // Apply mesh section with the specified id
	void ApplyMeshSection(uint64 MeshId, const FVoxelProcMeshSection& InSection);

private:

    typedef TPair<uint64, UVoxelProceduralMeshComponent*> FMeshPair;

    bool bOctreeLoaded;
    bool bGenerateMesh;
    bool bLoadCachedMesh;

    // Currently loaded LOD
    int32 LoadedLOD;

    // Render thread slot
    FThreadPoolSlotData ThreadSlot;

    // Polygonized voxel chunk nodes
	TQueue<FVoxelChunkNode*, EQueueMode::Mpsc> ChunksToApplyMesh;
	// Chunks waiting for update
	TQueue<FChunkOctree*> ChunksToUpdate;
	// Ids of the chunks that need to be updated synchronously
	TSet<FChunkOctree*> SynchronouslyUpdatingChunks;
	// Main octree reference
	TSharedPtr<FChunkOctree> MainOctree;

    // Cached mesh future object, valid if currently loading cached mesh data
    TFuture<TSharedPtr<FVoxelDBFetchData>> MeshCacheFetchFuture;
    // Cache fetch data persist until cached mesh have been constructed
    TSharedPtr<FVoxelDBFetchData> MeshCacheFetchData;
    // Registered cache builder task
	TSet<FVoxelCacheBuilder*> CacheBuilders;
    // Active cache builder task
	TQueue<FVoxelCacheBuilder*> RegisteredCacheBuilders;
    // Finished cache builder task
	TQueue<FVoxelCacheBuilder*, EQueueMode::Mpsc> FinishedCacheBuilders;

    // Render mesh component
    UVoxelLODMeshComponent* RenderMesh;

    // Collision mesh component
    UVoxelProceduralMeshComponent* CollisionMesh;

	// Invokers
	std::forward_list<TWeakObjectPtr<UVoxelInvokerComponent>> VoxelInvokerComponents;

	void LoadOctree();
	void UnloadOctree();
	void FetchCachedMesh();

    void LoadCachedMesh();
    void UnloadCacheBuilders();
    void UpdateCachedMeshLoading();
	void RegisterCacheLOD();
	void RegisterCacheUpdates();
    void ResetCacheQueue(bool bReserveSlack = true);

    void UpdateMeshGeneration();
	void RegisterChunkUpdates();
    void ResetUpdateQueue();

	void UpdateMesh();
    void StartProgressiveLOD();

	void CreateRenderMesh();
	void CreateCollisionMesh();
    FORCEINLINE bool HasLODSection(uint64 MeshId) const;
};
