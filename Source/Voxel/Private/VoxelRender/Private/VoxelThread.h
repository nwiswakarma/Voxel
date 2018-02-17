// Copyright 2017 Phyronnaz

#pragma once

#include "CoreMinimal.h"
#include "VoxelProceduralMeshTypes.h"

class FVoxelRender;
class FVoxelPolygonizer;
class FVoxelChunkNode;
struct FVoxelDBCacheData;

/**
 * Polygonizer task
 */
class FAsyncPolygonizerTask : public FNonAbandonableTask
{
	FVoxelChunkNode* const Chunk;
	TSharedPtr<FVoxelPolygonizer> Polygonizer;

public:

	FAsyncPolygonizerTask(FVoxelChunkNode* Chunk);
	~FAsyncPolygonizerTask();
	void DoWork();

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncPolygonizerTask, STATGROUP_ThreadPoolAsyncTasks);
	};
};

/**
 * Cache builder task instance
 */
class FVoxelCacheBuilder
{
    friend class FAsyncCacheBuilderTask;

	FVoxelRender* const Render;
	FVoxelDBCacheData const * const CacheData;
    FAsyncTask<class FAsyncCacheBuilderTask>* const Task;

    void OnWorkFinished();

public:
    FVoxelProcMeshSection Section;

	FVoxelCacheBuilder(FVoxelRender* const Render, FVoxelDBCacheData const * const CacheData);
	~FVoxelCacheBuilder();

	void StartBackgroundTask(FQueuedThreadPool* ThreadPool);
    FORCEINLINE void Reset();
    FORCEINLINE uint8 GetDepth() const;
    FORCEINLINE uint64 GetMeshId() const;
};

/**
 * Cache builder async task
 */
class FAsyncCacheBuilderTask : public FNonAbandonableTask
{
    FVoxelCacheBuilder* const Builder;

public:
    FThreadSafeBool bCanBuildMesh;

	FAsyncCacheBuilderTask(FVoxelCacheBuilder* const Builder);
	~FAsyncCacheBuilderTask();
	void DoWork();

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncCacheBuilderTask, STATGROUP_ThreadPoolAsyncTasks);
	};
};
