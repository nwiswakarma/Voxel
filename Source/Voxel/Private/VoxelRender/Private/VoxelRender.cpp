// Copyright 2017 Phyronnaz

#include "VoxelRender.h"
#include "ChunkOctree.h"
#include "VoxelChunkNode.h"
#include "VoxelDBCacheWorker.h"
#include "VoxelProceduralMeshComponent.h"
#include "VoxelMeshBuilder.h"
#include "VoxelThread.h"

#include "VoxelSave.h"
#include "NumericLimits.h"
#include "Kismet/GameplayStatics.h"
#include "DracoTypes.h"
#include "ZSTDTypes.h"

DECLARE_CYCLE_STAT(TEXT("VoxelRender ~ RegisterChunkUpdates"), STAT_RegisterChunkUpdates, STATGROUP_Voxel);
DECLARE_CYCLE_STAT(TEXT("VoxelRender ~ RegisterCacheUpdates"), STAT_RegisterCacheUpdates, STATGROUP_Voxel);
DECLARE_CYCLE_STAT(TEXT("VoxelRender ~ LoadOctree"), STAT_LoadOctree, STATGROUP_Voxel);
DECLARE_CYCLE_STAT(TEXT("VoxelRender ~ EncodeNodeMesh"), STAT_EncodeNodeMesh, STATGROUP_Voxel);
DECLARE_CYCLE_STAT(TEXT("VoxelRender ~ SaveEncodedNodeMesh"), STAT_SaveEncodedNodeMesh, STATGROUP_Voxel);

FVoxelRender::FVoxelRender(AVoxelWorld* World, AActor* ChunksParent, FVoxelData* Data)
	: World(World)
	, ChunksParent(ChunksParent)
	, Data(Data)
    , LoadedLOD(-1)
    , bOctreeLoaded(false)
    , bGenerateMesh(true)
    , bLoadCachedMesh(false)
    , RenderMesh(nullptr)
    , CollisionMesh(nullptr)
    , DBCacheWorker(nullptr)
    , RenderThreadPool( IVoxel::Get().GetRenderThreadPoolInstance() )
    , ThreadSlot( RenderThreadPool->CreateThreadSlot() )
    , MainOctree( new FChunkOctree(this, FIntVector::ZeroValue, Data->Depth, FOctree::GetTopIdFromDepth(Data->Depth), 0) )
{
    check(RenderThreadPool.IsValid());
}

FVoxelRender::~FVoxelRender()
{
	check(CacheBuilders.Num() == 0);
    check(! RenderThreadPool.IsValid());
    check(! DBCacheWorker.IsValid());
}

void FVoxelRender::Destroy()
{
    // Destroy mesh construction structures
    Unload();

    // Destroy main octree
    MainOctree.Reset();

    // Destroy render thread slot
    RenderThreadPool->DestroyThreadSlot(ThreadSlot);
	RenderThreadPool.Reset();

    // Clears mesh components
    RenderMesh = nullptr;

    // Clears collision mesh components
    CollisionMesh = nullptr;
}

void FVoxelRender::Load()
{
    // Already loaded, unload first
    if (LoadedLOD >= 0)
    {
        Unload();
    }

    LoadedLOD = World->GetLOD();

    ThreadSlot.Reset();
    CreateDBCacheWorker();

    // Generate mesh components with grouped LODs
    {
        const uint8 Depth = World->GetDepth();
        const uint8 MeshDepth = World->GetMeshDepth();
        const int32 MeshCount = 1<<((Depth-MeshDepth)*3);
        const uint64 TopID = FOctree::GetTopIdFromDepth(World->GetDepth());

        TArray<uint64> IDs;
        IDs.Reserve(MeshCount);

        // Get octree node ids at mesh depth
        FOctree::GetIDsAt(TopID, Depth, MeshDepth, IDs);

        // Create and register render mesh component
        CreateRenderMesh();
        check(RenderMesh);

        // Set render mesh LOD count
        RenderMesh->SetNumLODs(MeshDepth+1);

        // Create LOD Groups with mapped sections
        for (int32 LOD=0; LOD<RenderMesh->GetNumLODs(); ++LOD)
        {
            FVoxelProcMeshLOD& LODGroup( RenderMesh->GetLODGroup(LOD) );
            LODGroup.CreateMappedSections(IDs);
        }

        // Set mesh materials
        for (int32 i=0; i<RenderMesh->GetNumSections(); ++i)
        {
            RenderMesh->SetMaterial(i, World->GetVoxelMaterial());
        }

        // Generate collision mesh components if progressive LOD is enabled
        if (World->IsProgressiveLODEnabled())
        {
            // Disable collision on render meshes
            RenderMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

            // Initialize collision meshes
            CreateCollisionMesh();
            CollisionMesh->SetNumProcMeshSections(IDs.Num());
        }
    }

    // Generate mesh components
    {
        const uint8 Depth = World->GetDepth();
        const uint8 MeshDepth = World->GetMeshDepth();
        const int32 MeshCount = 1<<((Depth-MeshDepth)*3);
        const uint64 TopID = FOctree::GetTopIdFromDepth(World->GetDepth());

        TArray<uint64> IDs;
        IDs.Reserve(MeshCount);

        // Get octree node ids at mesh depth
        FOctree::GetIDsAt(TopID, Depth, MeshDepth, IDs);

        // Generate collision mesh components if progressive LOD is enabled
        if (World->IsProgressiveLODEnabled())
        {
        }
    }

    // Load from cache
    if (World->IsCachedMeshEnabled())
    {
        FetchCachedMesh();
    }

    // Construct mesh if required
    if (bGenerateMesh)
    {
        // Start progressive LOD loading if necessary
        if (World->IsProgressiveLODEnabled())
        {
            StartProgressiveLOD();
        }

        LoadOctree();
    }
}

void FVoxelRender::Unload()
{
    if (LoadedLOD >= 0)
    {
        LoadedLOD = -1;

        // Unload construction data
        UnloadOctree();
        UnloadCacheBuilders();

        // Reset render thread slot
        ThreadSlot.Reset();

        // Destroy database cache worker
        DBCacheWorker.Reset();

        // Clears all mesh sections

        if (RenderMesh)
        {
            RenderMesh->ClearLODGroups();
        }

        if (CollisionMesh)
        {
            CollisionMesh->ClearAllMeshSections();
        }
    }
}

void FVoxelRender::FetchCachedMesh()
{
    // Start data fetch if not currently doing so
    if (! MeshCacheFetchFuture.IsValid())
    {
        GetDBCacheWorker().FetchCacheData(MeshCacheFetchFuture);
        // Update cache loading flag
        bLoadCachedMesh = MeshCacheFetchFuture.IsValid();
        // Disable mesh generation update if cache loading is enabled
        bGenerateMesh = ! bLoadCachedMesh;
    }
}

void FVoxelRender::LoadOctree()
{
	SCOPE_CYCLE_COUNTER(STAT_LoadOctree);

    check(! bLoadCachedMesh);

	// Gather valid invoker components
	std::forward_list<TWeakObjectPtr<UVoxelInvokerComponent>> Invokers;
	for (auto Invoker : VoxelInvokerComponents)
	{
		if (Invoker.IsValid())
		{
			Invokers.push_front(Invoker);
		}
	}
	VoxelInvokerComponents = Invokers;

    ResetUpdateQueue();
    bOctreeLoaded = true;

	MainOctree->UpdateLOD();
}

void FVoxelRender::UnloadOctree()
{
    bOctreeLoaded = false;
    ResetUpdateQueue();

    MainOctree->Destroy();
}

void FVoxelRender::Tick(float DeltaTime)
{
    // Update mesh generation if not loading cached mesh
    if (bGenerateMesh)
    {
        UpdateMeshGeneration();
    }
    else if (bLoadCachedMesh)
    {
        if (MeshCacheFetchData.IsValid())
        {
            UpdateCachedMeshLoading();
        }
        else if (MeshCacheFetchFuture.IsReady())
        {
            LoadCachedMesh();
        }
    }
}

void FVoxelRender::LoadCachedMesh()
{
    check(MeshCacheFetchFuture.IsReady());

    // Move fetch future result
    TPSVoxelDBFetchData FetchData( MoveTemp(MeshCacheFetchFuture.Get()) );

    // Reset cache builders
    UnloadCacheBuilders();

    // Always treat cached mesh loading as loading progressive LOD
    StartProgressiveLOD();

    // Register cache construction
    MeshCacheFetchData = MoveTemp(FetchData);
    RegisterCacheLOD();
}

void FVoxelRender::UnloadCacheBuilders()
{
    ResetCacheQueue(false);

    // Invalidate fetch data
    MeshCacheFetchFuture = FVoxelDBFetchFuture();
    MeshCacheFetchData = TPSVoxelDBFetchData();
}

void FVoxelRender::UpdateCachedMeshLoading()
{
    // Task update started, mark update flag
    ThreadSlot.MarkTaskUpdate();

    while (! FinishedCacheBuilders.IsEmpty())
    {
        FVoxelCacheBuilder* Builder;
        FinishedCacheBuilders.Dequeue(Builder);
        uint64 MeshId = Builder->GetMeshId();

        // Apply mesh section
        if (HasLODSection(MeshId))
        {
            ApplyMeshSection(MeshId, Builder->Section);
        }

        Builder->Reset();
        ThreadSlot.DecrementActiveTask();
    }

    // Task update finished, unmark update flag
    const bool bChunkUpdateFinished = ThreadSlot.HasJustFinishedRemainingTask();
    ThreadSlot.UnmarkTaskUpdate();

    if (bChunkUpdateFinished)
    {
        UpdateMesh();

        if (LoadedLOD > 0)
        {
            //SetVisibleLOD(LoadedLOD);

            World->SetWorldLOD(--LoadedLOD);

            // Make sure cache builder containers are cleared
            ResetCacheQueue(true);

            // Load higher LOD
            RegisterCacheLOD();
        }
        // Unload octree after all LOD have been generated
        else
        {
            //SetVisibleLOD(0);

            // Finalize cache building
            UnloadCacheBuilders();
            bLoadCachedMesh = false;

            UE_LOG(LogTemp,Warning, TEXT("Progressive LOD Generation Finished!"));
        }
    }

    RegisterCacheUpdates();
}

void FVoxelRender::RegisterCacheLOD()
{
    // Make sure LOD is valid
    check(LoadedLOD >= 0);
    // Make sure cache queue is empty
    check(CacheBuilders.Num() == 0);
    check(RegisteredCacheBuilders.IsEmpty());
    check(FinishedCacheBuilders.IsEmpty());

    FVoxelDBCacheMap& CacheMap( MeshCacheFetchData->GetData() );
    const int32 LOD = LoadedLOD;

    for (const auto& CacheMapPair : CacheMap)
    {
        uint64 MeshId = CacheMapPair.Key;
        const FVoxelDBCacheLOD& CacheLODs( CacheMapPair.Value );

        if (! HasLODSection(MeshId) || ! CacheLODs.IsValidIndex(LOD))
        {
            continue;
        }

        const FVoxelDBCacheGroup& CacheGroup( CacheLODs[LOD] );

        // Construct cached section
        for (const FVoxelDBCacheData& CacheData : CacheGroup)
        {
            FVoxelCacheBuilder* Builder = new FVoxelCacheBuilder(this, &CacheData);
            CacheBuilders.Emplace(Builder);
            RegisteredCacheBuilders.Enqueue(Builder);
            ThreadSlot.IncrementTaskCount();
        }
    }
}

void FVoxelRender::RegisterCacheUpdates()
{
	SCOPE_CYCLE_COUNTER(STAT_RegisterCacheUpdates);

    while (! RegisteredCacheBuilders.IsEmpty() && ThreadSlot.HasRemainingTaskSlot())
    {
        FVoxelCacheBuilder* Builder;
        RegisteredCacheBuilders.Dequeue(Builder);

        // Start builder task
        Builder->StartBackgroundTask(GetRenderThreadPool());

        ThreadSlot.IncrementActiveTask();
    }
}

void FVoxelRender::ResetCacheQueue(bool bReserveSlack)
{
    // Destroy existing builders
    for (FVoxelCacheBuilder* Builder : CacheBuilders)
    {
        delete Builder;
    }

    // Clears builder containers
    if (bReserveSlack)
    {
        CacheBuilders.Reset();
    }
    else
    {
        CacheBuilders.Empty();
    }

    RegisteredCacheBuilders.Empty();
    FinishedCacheBuilders.Empty();

    // Reset thread work counter
    ThreadSlot.Reset();
}

void FVoxelRender::UpdateMeshGeneration()
{
    // Task update started, mark update flag
    ThreadSlot.MarkTaskUpdate();

	// Apply new meshes
    while (! ChunksToApplyMesh.IsEmpty())
    {
        FVoxelChunkNode* Chunk;
        ChunksToApplyMesh.Dequeue(Chunk);
        if (Chunk->IsValid())
            Chunk->ApplyMesh();
        ThreadSlot.DecrementActiveTask();
    }

    // Task update finished, unmark update flag
    const bool bChunkUpdateFinished = ThreadSlot.HasJustFinishedRemainingTask();
    ThreadSlot.UnmarkTaskUpdate();

    if (! World->IsAutoUpdateMesh())
    {
        // Updating chunks have all been updated, update mesh
        if (bChunkUpdateFinished)
        {
            UpdateMesh();

            // Generate progressive LOD
            if (World->IsProgressiveLODEnabled())
            {
                // Generate higher LOD
                if (LoadedLOD > 0)
                {
                    //SetVisibleLOD(LoadedLOD);

                    World->SetWorldLOD(--LoadedLOD);
                    LoadOctree();
                }
                // Unload octree after all LOD have been generated
                else
                {
                    //SetVisibleLOD(0);

                    GetDBCacheWorker().CommitCacheData();
                    UnloadOctree();
                    UE_LOG(LogTemp,Warning, TEXT("Progressive LOD Generation Finished!"));
                }
            }
        }
    }

	RegisterChunkUpdates();
}

void FVoxelRender::RegisterChunkUpdates()
{
	SCOPE_CYCLE_COUNTER(STAT_RegisterChunkUpdates);

    while (! ChunksToUpdate.IsEmpty() && ThreadSlot.HasRemainingTaskSlot())
    {
        FChunkOctree* Chunk;
        ChunksToUpdate.Dequeue(Chunk);

		if (Chunk->GetVoxelChunk())
		{
			bool bSuccess = Chunk->GetVoxelChunk()->Update(true);
		}

        ThreadSlot.IncrementActiveTask();
    }

	for (FChunkOctree* Chunk : SynchronouslyUpdatingChunks)
    {
		if (Chunk->GetVoxelChunk())
		{
			bool bSuccess = Chunk->GetVoxelChunk()->Update(false);
		}
    }
	SynchronouslyUpdatingChunks.Reset();
}

void FVoxelRender::ResetUpdateQueue()
{
    ChunksToApplyMesh.Empty();
    ChunksToUpdate.Empty();
    SynchronouslyUpdatingChunks.Empty();
    // Reset thread work counter
    ThreadSlot.Reset();
}

void FVoxelRender::UpdateAll(bool bAsync)
{
    MainOctree->UpdateChunk(bAsync, true);
}

void FVoxelRender::UpdateChunk(FChunkOctree* Chunk, bool bAsync)
{
	if (Chunk)
	{
		if (bAsync)
		{
            ChunksToUpdate.Enqueue(Chunk);
		}
        else
        {
			SynchronouslyUpdatingChunks.Add(Chunk);
        }

        ThreadSlot.IncrementTaskCount();
	}
}

void FVoxelRender::UpdateChunksAtPosition(FIntVector Position, bool bAsync)
{
	check(Data->IsInWorld(Position.X, Position.Y, Position.Z));

	UpdateChunksOverlappingBox(FVoxelBox(Position, Position), bAsync);
}

void FVoxelRender::UpdateChunksOverlappingBox(FVoxelBox Box, bool bAsync)
{
	Box.Min -= FIntVector(2, 2, 2); // For normals
	Box.Max += FIntVector(2, 2, 2); // For normals
	std::forward_list<FChunkOctree*> OverlappingLeafs;
	MainOctree->GetLeafsOverlappingBox(Box, OverlappingLeafs);

	for (auto Chunk : OverlappingLeafs)
	{
		UpdateChunk(Chunk, bAsync);
	}
}

void FVoxelRender::EnqueueMeshChunk(FVoxelChunkNode* Chunk)
{
    if (bOctreeLoaded)
    {
        ChunksToApplyMesh.Enqueue(Chunk);
    }
}

void FVoxelRender::EnqueueFinishedCacheBuilder(FVoxelCacheBuilder* Builder)
{
    FinishedCacheBuilders.Enqueue(Builder);
}

void FVoxelRender::EnqueueCacheData(TPSVoxelDBCacheData& CacheData)
{
    GetDBCacheWorker().EnqueueCacheData(CacheData);
}

void FVoxelRender::UpdateMesh()
{
    const bool bEnableCollision = World->GetComputeCollisions();
    const bool bProgressiveLOD = World->IsProgressiveLODEnabled();

    check(LoadedLOD >= 0);
    check(RenderMesh);
    check(RenderMesh->HasLODGroup(LoadedLOD));

    FVoxelProcMeshLOD& LODGroup(RenderMesh->GetLODGroup(LoadedLOD));
    const int32 SectionCount = LODGroup.GetNumSections();

    for (int32 i=0; i<SectionCount; ++i)
    {
        FVoxelProcMeshSection& Section( LODGroup.Sections[i] );
        Section.ProcVertexBuffer.Shrink();
        Section.ProcIndexBuffer.Shrink();
        Section.bSectionVisible = true;
        Section.bEnableCollision = bEnableCollision;
    }

    if (bEnableCollision && bProgressiveLOD)
    {
        const int32 LowestLOD = World->GetLowestProgressiveLOD();

        check(LowestLOD >= 0);

        if (LoadedLOD == 0 || LoadedLOD == LowestLOD)
        {
            check(CollisionMesh);
            check(CollisionMesh->GetNumSections() == SectionCount);

            const int32 CollisionLOD = LoadedLOD == 0 ? 0 : LowestLOD;

            for (int32 i=0; i<SectionCount; ++i)
            {
                FVoxelProcMeshSection& Section( LODGroup.Sections[i] );
                Section.bEnableCollision = false;
                // Construct collision section
                FVoxelProcMeshSection& CollisionSection( *CollisionMesh->GetProcMeshSection(i) );
                CollisionSection = Section;
                CollisionSection.bSectionVisible = false;
                CollisionSection.bEnableCollision = true;
            }

            // Update collision state
            CollisionMesh->UpdateRenderState();
        }
    }

    // Update render state
    RenderMesh->HighestLOD = LoadedLOD;
    RenderMesh->UpdateRenderState();
}

void FVoxelRender::StartProgressiveLOD()
{
    World->SetWorldLOD(World->GetMeshDepth());
    LoadedLOD = World->GetLOD();
}

bool FVoxelRender::HasMesh(uint64 MeshId) const
{
    return RenderMesh && (RenderMesh->GetMappedSection(0, MeshId) != nullptr);
}

FChunkOctree* FVoxelRender::GetOctree()
{
	return MainOctree.Get();
}

FChunkOctree* FVoxelRender::GetChunkOctreeAt(FIntVector Position) const
{
	check(Data->IsInWorld(Position.X, Position.Y, Position.Z));
	return MainOctree->GetLeaf(Position);
}

int FVoxelRender::GetDepthAt(FIntVector Position) const
{
	return GetChunkOctreeAt(Position)->Depth;
}

void FVoxelRender::SetVisibleLOD(int32 NewVisibleLOD)
{
    //const int32 LOD = FMath::Clamp<int>(NewVisibleLOD, 0, World->GetMeshDepth());

    //UE_LOG(LogTemp,Warning, TEXT("Visible LOD: %d"), LOD);

    //for (UVoxelProceduralMeshComponent* Mesh : ActiveMeshes)
    //{
    //    for (int32 i=0; i<Mesh->GetNumSections(); ++i)
    //    {
    //        if (i == LOD)
    //        {
    //            if (! Mesh->IsMeshSectionVisible(i))
    //            {
    //                Mesh->SetMeshSectionVisible(i, true);
    //            }
    //        }
    //        else if (Mesh->IsMeshSectionVisible(i))
    //        {
    //            Mesh->SetMeshSectionVisible(i, false);
    //        }
    //    }
    //}
}

void FVoxelRender::AddInvoker(TWeakObjectPtr<UVoxelInvokerComponent> Invoker)
{
	VoxelInvokerComponents.push_front(Invoker);
}

FVector FVoxelRender::GetGlobalPosition(FIntVector LocalPosition)
{
	return World->LocalToGlobal(LocalPosition) + ChunksParent->GetActorLocation() - World->GetActorLocation();
}

FQueuedThreadPool* const FVoxelRender::GetRenderThreadPool()
{
    check(RenderThreadPool.IsValid());
    return RenderThreadPool->GetThreadPool();
}

void FVoxelRender::ApplyMeshSection(uint64 MeshId, const FVoxelProcMeshSection& InSection)
{
    // Apply node mesh section
    check(RenderMesh);
    FVoxelMeshBuilder::ApplySection(InSection, *RenderMesh->GetMappedSection(LoadedLOD, MeshId));
}

void FVoxelRender::CreateRenderMesh()
{
	if (! RenderMesh)
	{
        UVoxelLODMeshComponent* Mesh = NewObject<UVoxelLODMeshComponent>(ChunksParent, NAME_None, RF_Transient | RF_NonPIEDuplicateTransient);

        // Setup attachment and register component
		Mesh->SetupAttachment(ChunksParent->GetRootComponent(), NAME_None);
		Mesh->RegisterComponent();

        // Setup mesh configuration
		Mesh->bUseAsyncCooking = World->IsAsyncCollisionCookingEnabled();
		Mesh->bUsePNTesselation = World->IsBuildPNTesselationEnabled();
		Mesh->bCalculateHighestLODBoundsOnly = true;
		Mesh->bCastShadowAsTwoSided = World->GetCastShadowAsTwoSided();
        Mesh->Mobility = EComponentMobility::Movable;
        Mesh->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
        Mesh->SetLODScreenSize(World->GetLODScreenSize());

        // Set component transform
        Mesh->SetRelativeLocation(FVector::ZeroVector);
        Mesh->SetWorldScale3D(FVector::OneVector * World->GetVoxelSize());

        RenderMesh = Mesh;
	}

    check(RenderMesh);
	check(RenderMesh->IsValidLowLevel());
}

void FVoxelRender::CreateCollisionMesh()
{
	if (! CollisionMesh)
	{
        UVoxelProceduralMeshComponent* Mesh = NewObject<UVoxelProceduralMeshComponent>(ChunksParent, NAME_None, RF_Transient | RF_NonPIEDuplicateTransient);

        // Setup attachment and register component
		Mesh->SetupAttachment(ChunksParent->GetRootComponent(), NAME_None);
		Mesh->RegisterComponent();

        // Setup mesh configuration
		Mesh->bUseAsyncCooking = World->IsAsyncCollisionCookingEnabled();
		Mesh->bUsePNTesselation = false;
        Mesh->Mobility = EComponentMobility::Movable;
        Mesh->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);

        // Set component transform
        Mesh->SetRelativeLocation(FVector::ZeroVector);
        Mesh->SetWorldScale3D(FVector::OneVector * World->GetVoxelSize());

        CollisionMesh = Mesh;
	}

    check(CollisionMesh);
	check(CollisionMesh->IsValidLowLevel());
}

bool FVoxelRender::HasLODSection(uint64 MeshId) const
{
    check(RenderMesh);
    return RenderMesh->GetMappedSection(LoadedLOD, MeshId) != nullptr;
}

void FVoxelRender::CreateDBCacheWorker()
{
    if (! DBCacheWorker.IsValid())
    {
        DBCacheWorker = IVoxel::Get().GetDBCacheWorker(World->GetWorldId());
    }
}

FVoxelDBCacheWorker& FVoxelRender::GetDBCacheWorker()
{
    check(DBCacheWorker.IsValid());
    return *DBCacheWorker.Get();
}
