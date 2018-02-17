// Copyright 2017 Phyronnaz

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VoxelSave.h"
#include "VoxelWorldGenerator.h"
#include "VoxelMaterial.h"
#include "VoxelBox.h"
#include "VoxelFoliage/VoxelGrassType.h"
#include "VoxelNetworking.h"
#include "VoxelWorld.generated.h"

using namespace UP;
using namespace UM;
using namespace US;
using namespace UC;

class FVoxelRender;
class FVoxelData;
class UVoxelInvokerComponent;

/**
 * Voxel World actor class
 */
UCLASS()
class VOXEL_API AVoxelWorld : public AActor
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Voxel")
		TArray<UVoxelGrassType*> GrassTypes;

	AVoxelWorld();
	virtual ~AVoxelWorld() override;

	void AddInvoker(TWeakObjectPtr<UVoxelInvokerComponent> Invoker);

	FORCEINLINE FVoxelData* GetData() const;
	FORCEINLINE UVoxelWorldGenerator* GetWorldGenerator() const;
	FORCEINLINE uint32 GetWorldId() const;
	FORCEINLINE int32 GetSeed() const;
	FORCEINLINE UMaterialInterface* GetVoxelMaterial() const;
	FORCEINLINE bool GetComputeTransitions() const;
	FORCEINLINE bool GetComputeCollisions() const;
	FORCEINLINE bool GetCastShadowAsTwoSided() const;
	FORCEINLINE float GetDeletionDelay() const;
	FORCEINLINE const TArray<float>& GetLODScreenSize() const;
    // Ambient Occlusion
	FORCEINLINE bool GetEnableAmbientOcclusion() const;
	FORCEINLINE int GetRayMaxDistance() const;
	FORCEINLINE int GetRayCount() const;
    // Mesh Compression
	FORCEINLINE bool GetEnableMeshCompression() const;
	FORCEINLINE int GetPositionQuantizationBits() const;
	FORCEINLINE int GetNormalQuantizationBits() const;
	FORCEINLINE int GetColorQuantizationBits() const;
	FORCEINLINE int GetMeshCompressionLevel() const;
	FORCEINLINE float GetNormalThresholdForSimplification() const;
    // Mesh Construction
	FORCEINLINE int GetLOD() const;
	FORCEINLINE int GetDepth() const;
	FORCEINLINE int GetMeshDepth() const;
	FORCEINLINE int GetLowestProgressiveLOD() const;
	FORCEINLINE bool IsAutoUpdateMesh() const;
	FORCEINLINE bool IsProgressiveLODEnabled() const;
	FORCEINLINE bool IsCachedMeshEnabled() const;
	FORCEINLINE bool IsAsyncCollisionCookingEnabled() const;
	FORCEINLINE bool IsBuildPNTesselationEnabled() const;

	FORCEINLINE FIntVector GetMinimalCornerPosition() const;
	FORCEINLINE FIntVector GetMaximalCornerPosition() const;


	UFUNCTION(BlueprintCallable, Category = "Voxel")
	UVoxelWorldGenerator* GetInstancedWorldGenerator() const
    {
        return InstancedWorldGenerator;
    }

	UFUNCTION(BlueprintCallable, Category = "Voxel")
		void LoadWorld();

	UFUNCTION(BlueprintCallable, Category = "Voxel")
		void UnloadWorld();

	UFUNCTION(BlueprintCallable, Category = "Voxel")
		bool IsCreated() const;

	UFUNCTION(BlueprintCallable, Category = "Voxel")
		int GetDepthAt(const FIntVector& Position) const;

	// Size of a voxel in cm
	UFUNCTION(BlueprintCallable, Category = "Voxel")
		float GetVoxelSize() const;

	// Size of this world
	UFUNCTION(BlueprintCallable, Category = "Voxel")
		int Size() const;

	// Draw debug voxel on an XY plane
	UFUNCTION(BlueprintCallable, Category = "Voxel")
		void DrawDebugVoxelXY(int32 Z);

	// Draw debug voxel on an XZ plane
	UFUNCTION(BlueprintCallable, Category = "Voxel")
		void DrawDebugVoxelXZ(int32 Y);

	// Draw debug voxel on an XZ plane
	UFUNCTION(BlueprintCallable, Category = "Voxel")
		void DrawDebugVoxelYZ(int32 X);

	/**
	 * Convert position from world space to voxel space
	 * @param	Position	Position in world space
	 * @return	Position in voxel space
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel")
		FIntVector GlobalToLocal(const FVector& Position) const;

	UFUNCTION(BlueprintCallable, Category = "Voxel")
		FVector LocalToGlobal(const FIntVector& Position) const;

	UFUNCTION(BlueprintCallable, Category = "Voxel")
		TArray<FIntVector> GetNeighboringPositions(const FVector& GlobalPosition) const;

	/**
	 * Add chunk to update queue that will be processed at the end of the frame
	 * @param	Position	Position in voxel space
	 * @param	bAsync		Async update?
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel")
		void UpdateChunksAtPosition(const FIntVector& Position, bool bAsync);

	UFUNCTION(BlueprintCallable, Category = "Voxel")
		void UpdateChunksOverlappingBox(const FVoxelBox& Box, bool bAsync);

	UFUNCTION(BlueprintCallable, Category = "Voxel")
		void UpdateAll(bool bAsync);

	/**
	 * Is position in this world?
	 * @param	Position	Position in voxel space
	 * @return	IsInWorld?
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel")
		bool IsInWorld(const FIntVector& Position) const;

	/**
	 * Get value at position
	 * @param	Position	Position in voxel space
	 * @return	Value at position
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel")
		float GetValue(const FIntVector& Position) const;
	/**
	 * Get material at position
	 * @param	Position	Position in voxel space
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel")
		FVoxelMaterial GetMaterial(const FIntVector& Position) const;

	/**
	 * Set value at position
	 * @param	Position	Position in voxel space
	 * @param	Value		Value to set
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel")
		void SetValue(const FIntVector& Position, float Value);
	/**
	 * Set material at position
	 * @param	Position	Position in voxel space
	 * @param	Material	FVoxelMaterial
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel")
		void SetMaterial(const FIntVector& Position, const FVoxelMaterial& Material);

	/**
	 * Set fixed LOD
	 * @param	Position	Position in voxel space
	 * @param	Value		Value to set
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel")
		void SetWorldLOD(int32 NewWorldLOD);

	/**
	 * Set mesh LOD depth
	 * @param	Position	Position in voxel space
	 * @param	Value		Value to set
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel")
		void SetMeshDepth(int32 NewMeshDepth);

	/**
	 * Get array to save world
	 * @return	SaveArray
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel")
		void GetSave(FVoxelWorldSave& OutSave) const;

	/**
	 * Save world mesh
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel")
		void SaveWorldMesh();
	/**
	 * Load world from save
	 * @param	Save	Save to load from
	 * @param	bReset	Reset existing world? Set to false only if current world is unmodified
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel")
		void LoadFromSave(FVoxelWorldSave& Save, bool bReset = true);


	UFUNCTION(BlueprintCallable, Category = "Voxel")
		void StartServer(const FString& Ip, const int32 Port);

	UFUNCTION(BlueprintCallable, Category = "Voxel")
		void ConnectClient(const FString& Ip, const int32 Port);


protected:
	// Called when the game starts or when spawned
	void BeginPlay() override;
	void Tick(float DeltaTime) override;
	void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(EditAnywhere, Category = "Voxel")
		UMaterialInterface* VoxelMaterial;

	// Width = 16 * 2^Depth
	UPROPERTY(EditAnywhere, Category = "Voxel", meta = (ClampMin = "0", ClampMax = "20", UIMin = "0", UIMax = "20", DisplayName = "Depth"))
		int NewDepth;

	// Size of a voxel in cm
	UPROPERTY(EditAnywhere, Category = "Voxel", meta = (DisplayName = "Voxel Size"))
		float NewVoxelSize;

	// Generator for this world
	UPROPERTY(EditAnywhere, Category = "Voxel")
		TSubclassOf<UVoxelWorldGenerator> WorldGenerator;

	UPROPERTY(EditAnywhere, Category = "Voxel")
		uint32 WorldId;

	UPROPERTY(EditAnywhere, Category = "Voxel", meta = (ClampMin = "1", UIMin = "1"))
		int32 Seed;

	UPROPERTY(EditAnywhere, Category = "Ambient Occlusion")
		bool bEnableAmbientOcclusion;

	UPROPERTY(EditAnywhere, Category = "Ambient Occlusion", meta = (EditCondition = "bEnableAmbientOcclusion"))
		int RayCount;

	UPROPERTY(EditAnywhere, Category = "Ambient Occlusion", meta = (EditCondition = "bEnableAmbientOcclusion"))
		int RayMaxDistance;

	UPROPERTY(EditAnywhere, Category = "Mesh Compression")
		bool bEnableMeshCompression;

	UPROPERTY(EditAnywhere, Category = "Mesh Compression", meta = (ClampMin = "1", ClampMax = "20", UIMin = "1", UIMax = "20", EditCondition = "bEnableMeshCompression"))
		int PositionQuantizationBits;

	UPROPERTY(EditAnywhere, Category = "Mesh Compression", meta = (ClampMin = "1", ClampMax = "20", UIMin = "1", UIMax = "20", EditCondition = "bEnableMeshCompression"))
		int NormalQuantizationBits;

	UPROPERTY(EditAnywhere, Category = "Mesh Compression", meta = (ClampMin = "1", ClampMax = "20", UIMin = "1", UIMax = "20", EditCondition = "bEnableMeshCompression"))
		int ColorQuantizationBits;

	UPROPERTY(EditAnywhere, Category = "Mesh Compression", meta = (ClampMin = "1", ClampMax = "10", UIMin = "1", UIMax = "10", EditCondition = "bEnableMeshCompression"))
		int MeshCompressionLevel;

	// Time to wait before deleting old chunks to avoid holes
	UPROPERTY(EditAnywhere, Category = "Voxel", meta = (ClampMin = "0", UIMin = "0"), AdvancedDisplay)
		float DeletionDelay;

	UPROPERTY(EditAnywhere, Category = "Voxel", AdvancedDisplay)
		bool bComputeTransitions;

	UPROPERTY(EditAnywhere, Category = "Voxel", AdvancedDisplay)
		bool bAutoLoadWorld;

	UPROPERTY(EditAnywhere, Category = "Voxel", AdvancedDisplay)
		bool bAutoUpdateMesh;

	UPROPERTY(EditAnywhere, Category = "Voxel", AdvancedDisplay)
		bool bEnableProgressiveLOD;

	UPROPERTY(EditAnywhere, Category = "Voxel", AdvancedDisplay)
		bool bEnableCachedMesh;

	UPROPERTY(EditAnywhere, Category = "Voxel", AdvancedDisplay)
		bool bUseAsyncCollisionCooking;

	UPROPERTY(EditAnywhere, Category = "Voxel", AdvancedDisplay)
		bool bBuildPNTesselation;

	UPROPERTY(EditAnywhere, Category = "Voxel", AdvancedDisplay, meta = (ClampMin = "0", ClampMax = "20", UIMin = "0", UIMax = "20"))
		int WorldLOD;

	UPROPERTY(EditAnywhere, Category = "Voxel", AdvancedDisplay, meta = (ClampMin = "0", ClampMax = "20", UIMin = "0", UIMax = "20", DisplayName = "Mesh Depth"))
		int NewMeshDepth;

	UPROPERTY(EditAnywhere, Category = "Voxel", AdvancedDisplay)
		float NormalThresholdForSimplification;

	UPROPERTY(EditAnywhere, Category = "Voxel", AdvancedDisplay)
		TArray<float> LODScreenSize;


	UPROPERTY(EditAnywhere, Category = "Multiplayer")
		bool bMultiplayer;

	UPROPERTY(EditAnywhere, Category = "Multiplayer", meta = (EditCondition = "bMultiplayer"))
		float MultiplayerSyncRate;


	UPROPERTY()
		UVoxelWorldGenerator* InstancedWorldGenerator;


	FVoxelTcpServer TcpServer;
	FVoxelTcpClient TcpClient;

	TSharedPtr<FVoxelData> Data;
	TSharedPtr<FVoxelRender> Render;

	bool bIsCreated;

	int Depth;
	int MeshDepth;
	int LowestProgressiveLOD;
	float VoxelSize;

	bool bComputeCollisions;
	bool bCastShadowAsTwoSided;

	float TimeSinceSync;

	void CreateWorld();
	void DestroyWorld();

	void Sync();
};
