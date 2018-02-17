// 

#include "VoxelMeshBuilder.h"

DECLARE_CYCLE_STAT(TEXT("VoxelMeshBuilder ~ ApplyOffset"), STAT_MeshBuilderApplyOffset, STATGROUP_Voxel);
DECLARE_CYCLE_STAT(TEXT("VoxelMeshBuilder ~ ApplySection"), STAT_MeshBuilderApplySection, STATGROUP_Voxel);

void FVoxelMeshBuilder::ApplyOffset(const FVector& Offset, FVoxelProcMeshSection& Section)
{
	SCOPE_CYCLE_COUNTER(STAT_MeshBuilderApplyOffset);

    TArray<FVoxelProcMeshVertex>& VB( Section.ProcVertexBuffer );

    // Apply vertex offset
    for (FVoxelProcMeshVertex& Vertex : VB)
    {
        Vertex.Position += Offset;
    }

    // Shift local bounding box
    Section.SectionLocalBox = Section.SectionLocalBox.ShiftBy(Offset);
}

void FVoxelMeshBuilder::ApplySection(const FVoxelProcMeshSection& SrcSection, FVoxelProcMeshSection& DstSection)
{
    // Source section is empty, abort
    if (SrcSection.ProcVertexBuffer.Num() <= 0 && SrcSection.ProcIndexBuffer.Num() <= 0)
    {
        return;
    }

	SCOPE_CYCLE_COUNTER(STAT_MeshBuilderApplySection);

    const TArray<FVoxelProcMeshVertex>& VB0( SrcSection.ProcVertexBuffer );
    TArray<FVoxelProcMeshVertex>& VB1( DstSection.ProcVertexBuffer );

    const TArray<int32>& IB0( SrcSection.ProcIndexBuffer );
    TArray<int32>& IB1( DstSection.ProcIndexBuffer );

    int32 VNum0 = VB0.Num();
    int32 VNum1 = VB1.Num();

    int32 INum0 = IB0.Num();
    int32 INum1 = IB1.Num();

    // Appends vertex buffer
    VB1.Reserve(VNum0+VNum1);
    VB1.Append(VB0);

    // Shift and append index buffer

    IB1.SetNumUninitialized(INum0+INum1);

    for (int32 i=0; i<INum0; ++i)
    {
        IB1[INum1+i] = IB0[i]+VNum1;
    }

    // Update target section local bounding box
    DstSection.SectionLocalBox.IsValid = true;
    DstSection.SectionLocalBox += SrcSection.SectionLocalBox;

    // Update mesh navigation data
    //UNavigationSystem::UpdateComponentInNavOctree(*this);
}

FBox FVoxelMeshBuilder::GetNodeBounds(uint8 Depth)
{
    const int32 s = Step(Depth);
    return FBox(-FVector::OneVector*s, 18*FVector::OneVector*s);
}
