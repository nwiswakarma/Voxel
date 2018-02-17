// Copyright 2017 Phyronnaz

#include "VoxelThread.h"
#include "VoxelChunkNode.h"
#include "VoxelPolygonizer.h"
#include "GenericPlatformProcess.h"

FAsyncPolygonizerTask::FAsyncPolygonizerTask(FVoxelChunkNode* Chunk)
	: Chunk(Chunk)
	, Polygonizer(Chunk->CreatePolygonizer())
{
}

FAsyncPolygonizerTask::~FAsyncPolygonizerTask()
{
    if (Polygonizer.IsValid())
    {
        Polygonizer.Reset();
    }
}

void FAsyncPolygonizerTask::DoWork()
{
    if (Chunk->CanCreateSection() && Polygonizer.IsValid())
    {
        FVoxelProcMeshSection Section;

        Polygonizer->CreateSection(Section);
        Polygonizer.Reset();

        Chunk->OnMeshComplete(Section);

        Section.Reset();
    }
}

FVoxelCacheBuilder::FVoxelCacheBuilder(FVoxelRender* const Render, FVoxelDBCacheData const * const CacheData)
	: Render(Render)
    , CacheData(CacheData)
    , Task(new FAsyncTask<FAsyncCacheBuilderTask>(this))
{
    check(Render);
    check(CacheData);
}

FVoxelCacheBuilder::~FVoxelCacheBuilder()
{
    // Reset mesh section
    Reset();
    // Destroy task
    Task->EnsureCompletion(false);
    delete Task;
}

void FVoxelCacheBuilder::StartBackgroundTask(FQueuedThreadPool* ThreadPool)
{
    check(ThreadPool);
    Task->StartBackgroundTask(ThreadPool);
}

void FVoxelCacheBuilder::Reset()
{
    Section.Reset();
}

void FVoxelCacheBuilder::OnWorkFinished()
{
    Render->EnqueueFinishedCacheBuilder(this);
}

uint8 FVoxelCacheBuilder::GetDepth() const
{
    return CacheData->Depth;
}

uint64 FVoxelCacheBuilder::GetMeshId() const
{
    return CacheData->MeshId;
}

FAsyncCacheBuilderTask::FAsyncCacheBuilderTask(FVoxelCacheBuilder* const Builder)
	: Builder(Builder)
    , bCanBuildMesh(true)
{
    check(Builder);
}

FAsyncCacheBuilderTask::~FAsyncCacheBuilderTask()
{
}

void FAsyncCacheBuilderTask::DoWork()
{
    if (bCanBuildMesh)
    {
        FVoxelProcMeshSection& Section(Builder->Section);
        const FVoxelDBCacheData& CacheData(*Builder->CacheData);

        // Decode cached mesh blob data
        FVoxelMeshEncoder MeshEncoder;
        MeshEncoder.DecodeMeshSection(CacheData.MeshData, Section);

        // Construct bounds and apply geometry offset
        Section.SectionLocalBox = FVoxelMeshBuilder::GetNodeBounds(CacheData.Depth);
        FVoxelMeshBuilder::ApplyOffset(FVector(CacheData.Offset), Section);

        Builder->OnWorkFinished();
    }
}
