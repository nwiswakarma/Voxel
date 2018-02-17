// Copyright 2017 Phyronnaz

#include "VoxelChunkNode.h"
#include "ChunkOctree.h"
#include "VoxelPolygonizer.h"
#include "VoxelRender.h"
#include "VoxelThread.h"
#include "VoxelDBCacheWorker.h"
#include "VoxelMeshEncoder.h"
#include "VoxelMeshBuilder.h"

#include "DracoTypes.h"
//#include "AI/Navigation/NavigationSystem.h"

DECLARE_CYCLE_STAT(TEXT("VoxelChunk ~ ApplyMesh"), STAT_ApplyMesh, STATGROUP_Voxel);
DECLARE_CYCLE_STAT(TEXT("VoxelChunk ~ OnMeshComplete"), STAT_OnMeshComplete, STATGROUP_Voxel);
DECLARE_CYCLE_STAT(TEXT("VoxelChunk ~ Update ~ Async"), STAT_UpdateAsync, STATGROUP_Voxel);
DECLARE_CYCLE_STAT(TEXT("VoxelChunk ~ Update ~ Sync"), STAT_UpdateSync, STATGROUP_Voxel);

// Sets default values
FVoxelChunkNode::FVoxelChunkNode(FChunkOctree* NewOctree)
	: CurrentOctree(NewOctree)
    , Render(NewOctree->Render)
    , Depth(NewOctree->Depth)
    , Offset(NewOctree->GetMinimalCornerPosition())
    , MeshId(NewOctree->GetMeshId())
	, MeshBuilderTask(nullptr)
    , bAbandonBuilder(false)
{
	ChunkHasHigherRes.SetNumZeroed(6);
}

FVoxelChunkNode::~FVoxelChunkNode()
{
    Destroy();
}

void FVoxelChunkNode::Destroy()
{
	EnsureTaskCompletion(true);

	// Reset mesh
    Section.Reset();
}

void FVoxelChunkNode::EnsureTaskCompletion(bool bCancel)
{
    // Ensure mesh builder task is complete then reset it
	if (MeshBuilderTask)
	{
        if (bCancel)
        {
            bAbandonBuilder = true;

            if (! MeshBuilderTask->Cancel())
            {
                MeshBuilderTask->EnsureCompletion(false);
            }
        }
        else
        {
            MeshBuilderTask->EnsureCompletion(false);
        }

		check(MeshBuilderTask->IsDone());
        delete MeshBuilderTask;
        MeshBuilderTask = nullptr;
	}
}

bool FVoxelChunkNode::Update(bool bAsync)
{
	check(Render);
	check(CurrentOctree);

    bool bUpdateSuccess = false;

	if (bAsync)
	{
		SCOPE_CYCLE_COUNTER(STAT_UpdateAsync);

        // Only update if there is no valid current task
		if (! MeshBuilderTask)
		{
            bAbandonBuilder = false;

			MeshBuilderTask = new FAsyncTask<FAsyncPolygonizerTask>(this);
            MeshBuilderTask->StartBackgroundTask(Render->GetRenderThreadPool());

			bUpdateSuccess = true;
		}
	}
	else
	{
		SCOPE_CYCLE_COUNTER(STAT_UpdateSync);

        EnsureTaskCompletion();

		TSharedPtr<FVoxelPolygonizer> Polygonizer( CreatePolygonizer() );
		Polygonizer->CreateSection(Section);
        Polygonizer.Reset();

        ApplyMeshOffset();
		ApplyMesh();

        bUpdateSuccess = true;
	}

    return bUpdateSuccess;
}

void FVoxelChunkNode::ApplyMesh()
{
	check(Render);
    check(CurrentOctree);

	SCOPE_CYCLE_COUNTER(STAT_ApplyMesh);

    EnsureTaskCompletion();

    //FVoxelProcMeshSection* MeshSection = Render->GetMeshSection(MeshId);
    //check(MeshSection);

    //// Apply node mesh section
    //FVoxelMeshBuilder::ApplySection(Section, *MeshSection);

    Render->ApplyMeshSection(MeshId, Section);

    // Clears section geometry buffers
    Section.Reset();
}

void FVoxelChunkNode::ApplyMeshOffset()
{
    FVoxelMeshBuilder::ApplyOffset(FVector(Offset), Section);
}

void FVoxelChunkNode::OnMeshComplete(FVoxelProcMeshSection& InSection)
{
	check(Render);
	check(CurrentOctree);
	check(MeshBuilderTask);

	SCOPE_CYCLE_COUNTER(STAT_OnMeshComplete);

    if (InSection.ProcVertexBuffer.Num() > 0)
    {
        if (Render->World->GetEnableMeshCompression())
        {
            TPSVoxelDBCacheData EncodedData( new FVoxelDBCacheData(Depth, MeshId, Offset) );

            const AVoxelWorld* World = Render->World;
            const int32 PQBits = World->GetPositionQuantizationBits();
            const int32 NQBits = World->GetNormalQuantizationBits();
            const int32 CQBits = World->GetColorQuantizationBits();
            const int32 CompressionLevel = World->GetMeshCompressionLevel();

            FVoxelMeshEncoder MeshEncoder(PQBits, NQBits, CQBits, CompressionLevel);
            TArray<uint8>& MeshData( EncodedData->MeshData );
            MeshData.Reset();
            // Encode mesh for persistent storage
            MeshEncoder.EncodeMeshSection(InSection, MeshData);
            // Immediately decode for mesh component section geometry
            MeshEncoder.DecodeMeshSection(MeshData, Section);
            MeshData.Shrink();

            Render->EnqueueCacheData(EncodedData);
        }
        else
        {
            Section.ProcVertexBuffer = MoveTemp(InSection.ProcVertexBuffer);
            Section.ProcIndexBuffer = MoveTemp(InSection.ProcIndexBuffer);
        }
    }

	Section.SectionLocalBox = InSection.SectionLocalBox;
	Section.bEnableCollision = InSection.bEnableCollision;
	Section.bSectionVisible = InSection.bSectionVisible;

    // Apply node offset to mesh geometry
    ApplyMeshOffset();

    // Section construction finished, register to mesh construction queue
	Render->EnqueueMeshChunk(this);
}

TSharedPtr<FVoxelPolygonizer> FVoxelChunkNode::CreatePolygonizer()
{
	check(Render);
	return MakeShareable(
        new FVoxelPolygonizer(
            Depth,
            Render->Data,
            Offset,
            ChunkHasHigherRes,
            Render->World->GetComputeTransitions(),
            Render->World->GetComputeCollisions(),
            Render->World->GetEnableAmbientOcclusion(),
            Render->World->GetRayMaxDistance(),
            Render->World->GetRayCount(),
            Render->World->GetNormalThresholdForSimplification()
        )
    );
}
