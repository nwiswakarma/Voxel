// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Interfaces/Interface_CollisionDataProvider.h"
#include "Components/MeshComponent.h"
#include "PhysicsEngine/ConvexElem.h"
#include "VoxelProceduralMeshTypes.h"
#include "VoxelLODMeshComponent.generated.h"

class FPrimitiveSceneProxy;

/**
*	Component that allows you to specify custom triangle mesh geometry
*	Beware! This feature is experimental and may be substantially changed in future releases.
*/
UCLASS(hidecategories = (Object, LOD), meta = (BlueprintSpawnableComponent), ClassGroup = Rendering)
class VOXEL_API UVoxelLODMeshComponent : public UMeshComponent, public IInterface_CollisionDataProvider
{
	GENERATED_UCLASS_BODY()

	/** 
	 *	Controls whether the complex (Per poly) geometry should be treated as 'simple' collision. 
	 *	Should be set to false if this component is going to be given simple collision and simulated.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Procedural Mesh")
	bool bUseComplexAsSimpleCollision;

	/**
	*	Controls whether the physics cooking should be done off the game thread. This should be used when collision geometry doesn't have to be immediately up to date (For example streaming in far away objects)
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Procedural Mesh")
	bool bUseAsyncCooking;

	/**
	*	Controls whether PN-AEN tesselation should be performed.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Procedural Mesh")
	bool bUsePNTesselation;

	/**
	*	Controls whether mesh bounds only calculated using the highest LOD bounds.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Procedural Mesh")
	bool bCalculateHighestLODBoundsOnly;

	/** Collision data */
	UPROPERTY(Instanced)
	class UBodySetup* ProcMeshBodySetup;

	/** LOD screen sizes */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Procedural Mesh")
	TArray<float> LODScreenSize;

    /** Highest LOD for visible LOD */
	int32 HighestLOD;

	/** Control visibility of a particular section */
	//UFUNCTION(BlueprintCallable, Category = "Components|ProceduralMesh")
	//void SetMeshSectionVisible(int32 SectionIndex, bool bNewVisibility);

	/** Returns whether a particular section is currently visible */
	//UFUNCTION(BlueprintCallable, Category = "Components|ProceduralMesh")
	//bool IsMeshSectionVisible(int32 SectionIndex) const;

	/** Returns number of sections currently created for this component */
	UFUNCTION(BlueprintCallable, Category = "Components|ProceduralMesh")
	int32 GetNumSections() const;

	//~ Begin Interface_CollisionDataProvider Interface
	virtual bool GetPhysicsTriMeshData(struct FTriMeshCollisionData* CollisionData, bool InUseAllTriData) override;
	virtual bool ContainsPhysicsTriMeshData(bool InUseAllTriData) const override;
	virtual bool WantsNegXTriMesh() override{ return false; }
	//~ End Interface_CollisionDataProvider Interface

	//~ Begin UPrimitiveComponent Interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual class UBodySetup* GetBodySetup() override;
	virtual UMaterialInterface* GetMaterialFromCollisionFaceIndex(int32 FaceIndex, int32& SectionIndex) const override;
	//~ End UPrimitiveComponent Interface.

	//~ Begin UMeshComponent Interface.
	virtual int32 GetNumMaterials() const override;
	//~ End UMeshComponent Interface.

	//~ Begin UObject Interface
	virtual void PostLoad() override;
	//~ End UObject Interface.

	/** Request render state update */
	void UpdateRenderState();

	/** Replace LOD screen sizes */
	void SetLODScreenSize(const TArray<float>& ScreenSize);

	FORCEINLINE FVoxelProcMeshSection* GetProcMeshSection(int32 LODIndex, int32 SectionIndex);
	FORCEINLINE FVoxelProcMeshSection* GetMappedSection(int32 LODIndex, uint64 MappedIndex);
	FORCEINLINE bool HasLODGroup(int32 LODIndex) const;
	FORCEINLINE int32 GetNumLODs() const;
	FORCEINLINE FVoxelProcMeshLOD& GetLODGroup(int32 LODIndex);
	FORCEINLINE const FVoxelProcMeshLOD& GetLODGroup(int32 LODIndex) const;

	void SetNumLODs(int32 LODCount, bool bAllowShrinking = true);
	void ClearLODGroups();

private:

	//~ Begin USceneComponent Interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	//~ End USceneComponent Interface.

	/** Update LocalBounds member from the local box of each section */
	void UpdateLocalBounds();
	/** Ensure ProcMeshBodySetup is allocated and configured */
	void CreateProcMeshBodySetup();
	/** Mark collision data as dirty, and re-create on instance if necessary */
	void UpdateCollision();
	/** Once async physics cook is done, create needed state */
	void FinishPhysicsAsyncCook(UBodySetup* FinishedBodySetup);

	/** Helper to create new body setup objects */
	UBodySetup* CreateBodySetupHelper();

	/** Mesh LOD groups */
	UPROPERTY()
	TArray<FVoxelProcMeshLOD> LODGroups;

	/** Convex shapes used for simple collision */
	UPROPERTY()
	TArray<FKConvexElem> CollisionConvexElems;

	/** Local space bounds of mesh */
	UPROPERTY()
	FBoxSphereBounds LocalBounds;
	
	/** Queue for async body setups that are being cooked */
	UPROPERTY(transient)
	TArray<UBodySetup*> AsyncBodySetupQueue;

    /** Clamped highest LOD */
    int32 ClampedHighestLOD;

	friend class FVoxelLODMeshSceneProxy;
};
