// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "VoxelProceduralMeshTypes.generated.h"

/**
*	Struct used to specify a tangent vector for a vertex
*	The Y tangent is computed from the cross product of the vertex normal (Tangent Z) and the TangentX member.
*/
USTRUCT(BlueprintType)
struct FVoxelProcMeshTangent
{
	GENERATED_USTRUCT_BODY()

	/** Direction of X tangent for this vertex */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Tangent)
	FVector TangentX;

	/** Bool that indicates whether we should flip the Y tangent when we compute it using cross product */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Tangent)
	bool bFlipTangentY;

	FVoxelProcMeshTangent()
		: TangentX(1.f, 0.f, 0.f)
		, bFlipTangentY(false)
	{}

	FVoxelProcMeshTangent(float X, float Y, float Z)
		: TangentX(X, Y, Z)
		, bFlipTangentY(false)
	{}

	FVoxelProcMeshTangent(FVector InTangentX, bool bInFlipTangentY)
		: TangentX(InTangentX)
		, bFlipTangentY(bInFlipTangentY)
	{}
};

/** One vertex for the procedural mesh, used for storing data internally */
USTRUCT(BlueprintType)
struct FVoxelProcMeshVertex
{
	GENERATED_USTRUCT_BODY()

	/** Vertex position */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Vertex)
	FVector Position;

	/** Vertex normal */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Vertex)
	FVector Normal;

	/** Vertex tangent */
	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Vertex)
	//FVoxelProcMeshTangent Tangent;

	/** Vertex color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Vertex)
	FColor Color;

	/** Vertex texture co-ordinate */
	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Vertex)
	//FVector2D UV0;


	FVoxelProcMeshVertex()
		: Position(0.f, 0.f, 0.f)
		, Normal(0.f, 0.f, 1.f)
		//, Tangent(FVector(1.f, 0.f, 0.f), false)
		, Color(255, 255, 255)
		//, UV0(0.f, 0.f)
	{}
};

/** One section of the procedural mesh. Each material has its own section. */
USTRUCT()
struct FVoxelProcMeshSection
{
	GENERATED_USTRUCT_BODY()

	/** Vertex buffer for this section */
	UPROPERTY()
	TArray<FVoxelProcMeshVertex> ProcVertexBuffer;

	/** Index buffer for this section */
	UPROPERTY()
	TArray<int32> ProcIndexBuffer;

	/** Local bounding box of section */
	UPROPERTY()
	FBox SectionLocalBox;

	/** Should we build collision data for triangles in this section */
	UPROPERTY()
	bool bEnableCollision;

	/** Should we display this section */
	UPROPERTY()
	bool bSectionVisible;

	FVoxelProcMeshSection()
		: SectionLocalBox(ForceInit)
		, bEnableCollision(false)
		, bSectionVisible(true)
	{}

	/** Reset this section, clear all mesh info */
	void Reset()
	{
		ProcVertexBuffer.Empty();
		ProcIndexBuffer.Empty();
		SectionLocalBox.Init();
		bEnableCollision = false;
		bSectionVisible = true;
	}
};

USTRUCT()
struct FVoxelProcMeshLOD
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
    TArray<FVoxelProcMeshSection> Sections;

	UPROPERTY()
    TMap<uint64, int32> SectionMap;

    FORCEINLINE bool HasSection(int32 SectionIndex) const
    {
        return Sections.IsValidIndex(SectionIndex);
    }

    FORCEINLINE bool HasMapped(uint64 MappedIndex) const
    {
        return SectionMap.Contains(MappedIndex) ? HasSection(SectionMap.FindChecked(MappedIndex)) : false;
    }

    FORCEINLINE FVoxelProcMeshSection* GetSectionSafe(int32 SectionIndex)
    {
        return HasSection(SectionIndex) ? &GetSection(SectionIndex) : nullptr;
    }

    FORCEINLINE FVoxelProcMeshSection* GetMappedSafe(uint64 MappedIndex)
    {
        return HasMapped(MappedIndex) ? &GetMapped(MappedIndex) : nullptr;
    }

    FORCEINLINE FVoxelProcMeshSection& GetSection(int32 SectionIndex)
    {
        return Sections[SectionIndex];
    }

    FORCEINLINE FVoxelProcMeshSection& GetMapped(uint64 MappedIndex)
    {
        return Sections[SectionMap.FindChecked(MappedIndex)];
    }

    FORCEINLINE int32 GetNumSections() const
    {
        return Sections.Num();
    }

    FORCEINLINE FBox GetLocalBounds() const
    {
        FBox LocalBox(ForceInit);
        
        for (const FVoxelProcMeshSection& Section : Sections)
        {
            LocalBox += Section.SectionLocalBox;
        }
        
        return LocalBox.IsValid ? LocalBox : FBox(FVector::ZeroVector, FVector::ZeroVector); // fallback to reset box sphere bounds
    }

    void ClearAllMeshSections()
    {
        Sections.Empty();
        SectionMap.Empty();
    }

    void CreateMappedSections(const TArray<uint64>& SectionIds)
    {
        // Resize section container
        Sections.SetNum(SectionIds.Num(), true);

        // Map sections
        for (int32 i=0; i<SectionIds.Num(); ++i)
        {
            SectionMap.Emplace(SectionIds[i], i);
        }
    }
};
