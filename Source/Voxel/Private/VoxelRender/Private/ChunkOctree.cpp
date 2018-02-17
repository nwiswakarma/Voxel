// 

#include "ChunkOctree.h"
#include "Math/NumericLimits.h"
#include "VoxelInvokerComponent.h"
#include "VoxelProceduralMeshComponent.h"
#include "VoxelRender.h"
#include "VoxelChunkNode.h"

FChunkOctree::FChunkOctree(FVoxelRender* Render, FIntVector Position, uint8 Depth, uint64 Id, uint64 MeshId)
	: FOctree(Position, Depth, Id)
	, MeshId(MeshId)
	, Render(Render)
	, bHasChunk(false)
	, VoxelChunk(nullptr)
{
	check(Render);
}

FChunkOctree::~FChunkOctree()
{
    Destroy();
}

void FChunkOctree::Destroy()
{
	if (bHasChunk)
	{
		Unload();
	}

	if (bHasChilds)
	{
		DestroyChilds();
	}

    ResetMeshId();
}

void FChunkOctree::Load()
{
	check(!VoxelChunk);
	check(!bHasChunk);
	check(!bHasChilds);

    AssignMeshId();

	VoxelChunk = new FVoxelChunkNode(this);
	Render->UpdateChunk(this, true);

	bHasChunk = true;
}

void FChunkOctree::Unload()
{
	check(VoxelChunk);
	check(bHasChunk);
	check(!bHasChilds);

	delete VoxelChunk;
	VoxelChunk = nullptr;

	bHasChunk = false;
}

void FChunkOctree::CreateChilds()
{
	check(!bHasChilds);
	check(!bHasChunk);
	check(Depth != 0);

    uint64 IDs[8];
	const int32 d = Size()/4;
    const uint8 LOD = Depth-1;

    AssignMeshId();

    FOctree::GetIDsAt(Id, LOD, IDs);
	Childs.Add(new FChunkOctree(Render, Position+FIntVector(-d,-d,-d), LOD, IDs[0], MeshId));
	Childs.Add(new FChunkOctree(Render, Position+FIntVector(+d,-d,-d), LOD, IDs[1], MeshId));
	Childs.Add(new FChunkOctree(Render, Position+FIntVector(-d,+d,-d), LOD, IDs[2], MeshId));
	Childs.Add(new FChunkOctree(Render, Position+FIntVector(+d,+d,-d), LOD, IDs[3], MeshId));
	Childs.Add(new FChunkOctree(Render, Position+FIntVector(-d,-d,+d), LOD, IDs[4], MeshId));
	Childs.Add(new FChunkOctree(Render, Position+FIntVector(+d,-d,+d), LOD, IDs[5], MeshId));
	Childs.Add(new FChunkOctree(Render, Position+FIntVector(-d,+d,+d), LOD, IDs[6], MeshId));
	Childs.Add(new FChunkOctree(Render, Position+FIntVector(+d,+d,+d), LOD, IDs[7], MeshId));

	bHasChilds = true;
}

void FChunkOctree::DestroyChilds()
{
	check(!bHasChunk);
	check(bHasChilds);
	check(Childs.Num() == 8);

	for (FChunkOctree* Child : Childs)
	{
        delete Child;
	}

	Childs.Reset();
	bHasChilds = false;
}

void FChunkOctree::UpdateChunk(bool bAsync, bool bRecursive)
{
    check(Render);

    if (bHasChunk)
    {
        Render->UpdateChunk(this, bAsync);
    }

    if (bRecursive && bHasChilds)
    {
        for (FChunkOctree* Child : Childs)
            Child->UpdateChunk(bAsync, bRecursive);
    }
}

void FChunkOctree::UpdateLOD()
{
	check(bHasChunk == (VoxelChunk != nullptr));
	check(bHasChilds == (Childs.Num() == 8));
	check(!(bHasChilds && bHasChunk));

	if (Depth == 0)
	{
		// Always create
		if (!bHasChunk)
		{
			Load();
		}
		return;
	}

	const int32 MinLOD = Render->World->GetLOD();
	const int32 MaxLOD = MinLOD;

	if (MinLOD < Depth && Depth < MaxLOD)
	{
		// Depth OK
		if (bHasChilds)
		{
			// Update childs
			for (int i = 0; i < 8; i++)
			{
				Childs[i]->UpdateLOD();
			}
		}
		else if (!bHasChunk)
		{
			// Not created, create
			Load();
		}
	}
	else if (MaxLOD < Depth)
	{
		// Resolution too low
		if (bHasChunk)
		{
			Unload();
		}
		if (!bHasChilds)
		{
			CreateChilds();
		}
		if (bHasChilds)
		{
			// Update childs
			for (int i = 0; i < 8; i++)
			{
				Childs[i]->UpdateLOD();
			}
		}
	}
	else // Depth < MinLOD
	{
		// Resolution too high
		if (bHasChilds)
		{
			// Too far, delete childs
			DestroyChilds();
		}
		if (!bHasChunk)
		{
			// Not created, create
			Load();
		}
	}
}

FChunkOctree* FChunkOctree::GetLeaf(FIntVector PointPosition)
{
	check(bHasChunk == (VoxelChunk != nullptr));
	check(bHasChilds == (Childs.Num() == 8));

    FChunkOctree* Current = this;

	while (!Current->bHasChunk)
	{
		Current = Current->GetChild(PointPosition);
	}

    check(Current);
    check(Current->bHasChunk);

	return Current;
}

FVoxelChunkNode* FChunkOctree::GetVoxelChunk() const
{
	return VoxelChunk;
}

void FChunkOctree::AssignMeshId()
{
    if (MeshId == 0)
    {
        MeshId = Render->HasMesh(Id) ? Id : 0;
    }
}

void FChunkOctree::ResetMeshId()
{
    MeshId = 0;
}

uint64 FChunkOctree::GetMeshId() const
{
    return MeshId;
}

FChunkOctree* FChunkOctree::GetChild(FIntVector PointPosition)
{
	check(bHasChilds);
	check(IsInOctree(PointPosition.X, PointPosition.Y, PointPosition.Z));

	// Ex: Child 6 -> position (0, 1, 1) -> 0b011 == 6
	FChunkOctree* Child = Childs[
          (PointPosition.X >= Position.X ? 1 : 0)
        + (PointPosition.Y >= Position.Y ? 2 : 0)
        + (PointPosition.Z >= Position.Z ? 4 : 0)
    ];

	return Child;
}

void FChunkOctree::GetLeafsOverlappingBox(FVoxelBox Box, std::forward_list<FChunkOctree*>& Octrees)
{
	FVoxelBox OctreeBox(GetMinimalCornerPosition(), GetMaximalCornerPosition());

	if (OctreeBox.Intersect(Box))
	{
		if (IsLeaf())
		{
			Octrees.push_front(this);
		}
		else
		{
			for (FChunkOctree* Child : Childs)
			{
				Child->GetLeafsOverlappingBox(Box, Octrees);
			}
		}
	}
}
