// 

#pragma once

#include "CoreMinimal.h"
#include "VoxelProceduralMeshTypes.h"

/**
 * Voxel mesh builder
 */
class FVoxelMeshBuilder
{
    FORCEINLINE static int32 Step(uint8 Depth)
    {
        return 1<<Depth;
    }

public:
    static void ApplyOffset(const FVector& Offset, FVoxelProcMeshSection& Section);
    static void ApplySection(const FVoxelProcMeshSection& SrcSection, FVoxelProcMeshSection& DstSection);
    FORCEINLINE static FBox GetNodeBounds(uint8 Depth);
};
