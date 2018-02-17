// 

#pragma once

#include "CoreMinimal.h"
#include "VoxelProceduralMeshTypes.h"

namespace draco
{
    class Encoder;
    class EncoderBuffer;
    class Decoder;
    class DecoderBuffer;
}

/**
 * Voxel mesh encoder
 */
class FVoxelMeshEncoder
{
    int32 PositionQuantizationBits = 14;
    int32 NormalQuantizationBits = 10;
    int32 ColorQuantizationBits = 4;
    int32 CompressionSpeed = 3;

public:
    FVoxelMeshEncoder() = default;
    FVoxelMeshEncoder(int32 InPositionQuantizationBits, int32 InNormalQuantizationBits, int32 InColorQuantizationBits, int32 InCompressionLevel);
	void EncodeMeshSection(const FVoxelProcMeshSection& Section, TArray<uint8>& ByteData);
	void DecodeMeshSection(const TArray<uint8>& ByteData, FVoxelProcMeshSection& Section);
};
