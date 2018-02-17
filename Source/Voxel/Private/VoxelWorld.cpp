// Copyright 2017 Phyronnaz

#include "VoxelWorld.h"
#include "Components/CapsuleComponent.h"
#include "VoxelData.h"
#include "VoxelRender.h"
#include "VoxelInvokerComponent.h"
#include "FlatWorldGenerator.h"
#include <forward_list>

#include "DrawDebugHelpers.h"

AVoxelWorld::AVoxelWorld()
	: NewDepth(9)
    , NewMeshDepth(0)
	, WorldLOD(0)
	, LowestProgressiveLOD(-1)
	, DeletionDelay(0.1f)
	, bComputeTransitions(false)
	, bEnableProgressiveLOD(false)
	, bAutoLoadWorld(true)
	, bAutoUpdateMesh(false)
	, bEnableCachedMesh(false)
	, bUseAsyncCollisionCooking(true)
	, bBuildPNTesselation(false)
	, bIsCreated(false)
	, NewVoxelSize(100)
	, WorldId(0)
	, Seed(100)
	, bMultiplayer(false)
	, MultiplayerSyncRate(10)
	, Render(nullptr)
	, Data(nullptr)
	, InstancedWorldGenerator(nullptr)
	, bComputeCollisions(true)
	, bCastShadowAsTwoSided(false)
	, bEnableAmbientOcclusion(false)
	, RayMaxDistance(5)
	, RayCount(25)
	, bEnableMeshCompression(false)
	, PositionQuantizationBits(14)
	, NormalQuantizationBits(10)
	, ColorQuantizationBits(4)
	, MeshCompressionLevel(7)
	, NormalThresholdForSimplification(1.f)
	, TimeSinceSync(0)
{
	PrimaryActorTick.bCanEverTick = true;

	UCapsuleComponent* TouchCapsule = CreateDefaultSubobject<UCapsuleComponent>(FName("Capsule"));
	TouchCapsule->InitCapsuleSize(0.1f, 0.1f);
	TouchCapsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TouchCapsule->SetCollisionResponseToAllChannels(ECR_Ignore);
	RootComponent = TouchCapsule;

	WorldGenerator = TSubclassOf<UVoxelWorldGenerator>(UFlatWorldGenerator::StaticClass());
}

AVoxelWorld::~AVoxelWorld()
{
	if (IsCreated())
	{
        DestroyWorld();
	}
}

void AVoxelWorld::BeginPlay()
{
	Super::BeginPlay();

	if (!IsCreated())
	{
		CreateWorld();
	}
}

void AVoxelWorld::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsCreated())
	{
        DestroyWorld();
	}

	Super::EndPlay(EndPlayReason);
}

void AVoxelWorld::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (IsCreated())
	{
		Render->Tick(DeltaTime);
	}

	if (bMultiplayer && (TcpClient.IsValid() || TcpServer.IsValid()))
	{
		TimeSinceSync += DeltaTime;
		if (TimeSinceSync > 1.f / MultiplayerSyncRate)
		{
			TimeSinceSync = 0;
			Sync();
		}
	}
}

bool AVoxelWorld::IsCreated() const
{
	return bIsCreated;
}

void AVoxelWorld::CreateWorld()
{
	check(!IsCreated());

	UE_LOG(LogVoxel, Warning, TEXT("Creating world"));

	Depth = NewDepth;
	VoxelSize = NewVoxelSize;

    SetMeshDepth(NewMeshDepth);

    // Clamp world LOD if higher than mesh LOD
    if (WorldLOD > MeshDepth)
    {
        WorldLOD = MeshDepth;
    }

    // Lowest possible progressive LOD depth
    LowestProgressiveLOD = IsProgressiveLODEnabled() ? WorldLOD : MeshDepth;

	SetActorScale3D(FVector::OneVector);

	check(!Data.IsValid());
	check(!Render.IsValid());

	if (!InstancedWorldGenerator || InstancedWorldGenerator->GetClass() != WorldGenerator->GetClass())
	{
		// Create generator
		InstancedWorldGenerator = NewObject<UVoxelWorldGenerator>((UObject*)GetTransientPackage(), WorldGenerator);
		if (InstancedWorldGenerator == nullptr)
		{
			UE_LOG(LogVoxel, Error, TEXT("Invalid world generator"));
			InstancedWorldGenerator = NewObject<UVoxelWorldGenerator>((UObject*)GetTransientPackage(), UFlatWorldGenerator::StaticClass());
		}
	}

	// Create Data
	Data = MakeShareable( new FVoxelData(Depth, InstancedWorldGenerator, bMultiplayer) );

	// Create Render
	Render = MakeShareable( new FVoxelRender(this, this, Data.Get()) );

	InstancedWorldGenerator->SetVoxelWorld(this);

    if (bAutoLoadWorld)
    {
        Render->Load();
    }

	bIsCreated = true;
}

void AVoxelWorld::DestroyWorld()
{
	check(IsCreated());

	UE_LOG(LogVoxel, Warning, TEXT("Destroying world"));

	check(Render.IsValid());
	check(Data.IsValid());
	Render->Destroy();
	Render.Reset();
	Data.Reset(); // Data must be deleted AFTER Render

	bIsCreated = false;
}

void AVoxelWorld::LoadWorld()
{
    check(IsCreated());
    Render->Load();
}

void AVoxelWorld::UnloadWorld()
{
    check(IsCreated());
    Render->Unload();
}

void AVoxelWorld::UpdateChunksAtPosition(const FIntVector& Position, bool bAsync)
{
	Render->UpdateChunksAtPosition(Position, bAsync);
}

void AVoxelWorld::UpdateChunksOverlappingBox(const FVoxelBox& Box, bool bAsync)
{
	Render->UpdateChunksOverlappingBox(Box, bAsync);
}

void AVoxelWorld::UpdateAll(bool bAsync)
{
	Render->UpdateAll(bAsync);
}

void AVoxelWorld::AddInvoker(TWeakObjectPtr<UVoxelInvokerComponent> Invoker)
{
	Render->AddInvoker(Invoker);
}

void AVoxelWorld::StartServer(const FString& Ip, const int32 Port)
{
	TcpServer.StartTcpServer(Ip, Port);
}

void AVoxelWorld::ConnectClient(const FString& Ip, const int32 Port)
{
	TcpClient.ConnectTcpClient(Ip, Port);
}

void AVoxelWorld::Sync()
{
	if (TcpServer.IsValid())
	{
		FBufferArchive ToBinary;

		std::forward_list<FVoxelValueDiff> ValueDiffList;
		std::forward_list<FVoxelMaterialDiff> MaterialDiffList;
		Data->GetDiffLists(ValueDiffList, MaterialDiffList);

		int ValueDiffCount = 0;
		int MaterialDiffCount = 0;
		for (auto ValueDiff : ValueDiffList)
		{
			ValueDiffCount++;
		}
		for (auto MaterialDiff : MaterialDiffList)
		{
			MaterialDiffCount++;
		}

		ToBinary << ValueDiffCount;
		ToBinary << MaterialDiffCount;
		for (auto ValueDiff : ValueDiffList)
		{
			ToBinary << ValueDiff;
		}
		for (auto MaterialDiff : MaterialDiffList)
		{
			ToBinary << MaterialDiff;
		}

		bool bSuccess = TcpServer.SendData(ToBinary);
		if (!bSuccess)
		{
			UE_LOG(LogTemp, Error, TEXT("SendData failed"));
		}
	}
	else if (TcpClient.IsValid())
	{
		TArray<uint8> BinaryData;
		TcpClient.ReceiveData(BinaryData);

		if (BinaryData.Num())
		{
			FMemoryReader FromBinary(BinaryData);
			FromBinary.Seek(0);

			std::forward_list<FVoxelValueDiff> ValueDiffList;
			std::forward_list<FVoxelMaterialDiff> MaterialDiffList;

			int ValueDiffCount = 0;
			int MaterialDiffCount = 0;
			FromBinary << ValueDiffCount;
			FromBinary << MaterialDiffCount;

			for (int i = 0; i < ValueDiffCount; i++)
			{
				FVoxelValueDiff ValueDiff;
				FromBinary << ValueDiff;
				ValueDiffList.push_front(ValueDiff);
			}
			for (int i = 0; i < MaterialDiffCount; i++)
			{
				FVoxelMaterialDiff MaterialDiff;
				FromBinary << MaterialDiff;
				MaterialDiffList.push_front(MaterialDiff);
			}

			std::forward_list<FIntVector> ModifiedPositions;
			Data->LoadFromDiffListsAndGetModifiedPositions(ValueDiffList, MaterialDiffList, ModifiedPositions);

			for (auto Position : ModifiedPositions)
			{
				UpdateChunksAtPosition(Position, true);
				DrawDebugPoint(GetWorld(), LocalToGlobal(Position), 10, FColor::Magenta, false, 1.1f / MultiplayerSyncRate);
			}
		}
	}
	else
	{
		UE_LOG(LogVoxel, Error, TEXT("No valid TCPSender/TCPListener"));
	}
}

float AVoxelWorld::GetValue(const FIntVector& Position) const
{
	if (IsInWorld(Position))
	{
		FVoxelMaterial Material;
		float Value;

		Data->BeginGet();
		Data->GetValueAndMaterial(Position.X, Position.Y, Position.Z, Value, Material);
		Data->EndGet();

		return Value;
	}
	else
	{
		UE_LOG(LogVoxel, Error, TEXT("Get value: Not in world: (%d, %d, %d)"), Position.X, Position.Y, Position.Z);
		return 0;
	}
}

FVoxelMaterial AVoxelWorld::GetMaterial(const FIntVector& Position) const
{
	if (IsInWorld(Position))
	{
		FVoxelMaterial Material;
		float Value;

		Data->BeginGet();
		Data->GetValueAndMaterial(Position.X, Position.Y, Position.Z, Value, Material);
		Data->EndGet();

		return Material;
	}
	else
	{
		UE_LOG(LogVoxel, Error, TEXT("Get material: Not in world: (%d, %d, %d)"), Position.X, Position.Y, Position.Z);
		return FVoxelMaterial();
	}
}

void AVoxelWorld::SetValue(const FIntVector& Position, float Value)
{
	if (IsInWorld(Position))
	{
		Data->BeginSet();
		Data->SetValue(Position.X, Position.Y, Position.Z, Value);
		Data->EndSet();
	}
	else
	{
		UE_LOG(LogVoxel, Error, TEXT("Get material: Not in world: (%d, %d, %d)"), Position.X, Position.Y, Position.Z);
	}
}

void AVoxelWorld::SetMaterial(const FIntVector& Position, const FVoxelMaterial& Material)
{
	if (IsInWorld(Position))
	{
		Data->BeginSet();
		Data->SetMaterial(Position.X, Position.Y, Position.Z, Material);
		Data->EndSet();
	}
	else
	{
		UE_LOG(LogVoxel, Error, TEXT("Set material: Not in world: (%d, %d, %d)"), Position.X, Position.Y, Position.Z);
	}
}

void AVoxelWorld::SetWorldLOD(int32 NewWorldLOD)
{
    WorldLOD = NewWorldLOD;
}

void AVoxelWorld::SetMeshDepth(int32 NewMeshDepth)
{
    MeshDepth = Depth - FMath::Clamp<int>(NewMeshDepth, 0, Depth);
}

void AVoxelWorld::GetSave(FVoxelWorldSave& OutSave) const
{
	Data->GetSave(OutSave);
}

void AVoxelWorld::SaveWorldMesh()
{
	check(Render.IsValid());
    //Render->GetMeshSaveData();
}

void AVoxelWorld::LoadFromSave(FVoxelWorldSave& Save, bool bReset)
{
	if (Save.Depth == Depth)
	{
		std::forward_list<FIntVector> ModifiedPositions;
		Data->LoadFromSaveAndGetModifiedPositions(Save, ModifiedPositions, bReset);
		for (auto Position : ModifiedPositions)
		{
			if (IsInWorld(Position))
			{
				UpdateChunksAtPosition(Position, true);
			}
		}
		//Render->RegisterChunkUpdates();
	}
	else
	{
		UE_LOG(LogVoxel, Error, TEXT("LoadFromSave: Current Depth is %d while Save one is %d"), Depth, Save.Depth);
	}
}

FVoxelData* AVoxelWorld::GetData() const
{
	return Data.Get();
}

UVoxelWorldGenerator* AVoxelWorld::GetWorldGenerator() const
{
	return InstancedWorldGenerator;
}

uint32 AVoxelWorld::GetWorldId() const
{
	return WorldId;
}

int32 AVoxelWorld::GetSeed() const
{
	return Seed;
}

UMaterialInterface* AVoxelWorld::GetVoxelMaterial() const
{
	return VoxelMaterial;
}

bool AVoxelWorld::GetComputeTransitions() const
{
	return bComputeTransitions;
}

bool AVoxelWorld::GetComputeCollisions() const
{
	return bComputeCollisions;
}

bool AVoxelWorld::GetCastShadowAsTwoSided() const
{
	return bCastShadowAsTwoSided;
}

float AVoxelWorld::GetDeletionDelay() const
{
	return DeletionDelay;
}

const TArray<float>& AVoxelWorld::GetLODScreenSize() const
{
	return LODScreenSize;
}

bool AVoxelWorld::GetEnableAmbientOcclusion() const
{
	return bEnableAmbientOcclusion;
}

int AVoxelWorld::GetRayMaxDistance() const
{
	return RayMaxDistance;
}

int AVoxelWorld::GetRayCount() const
{
	return RayCount;
}

bool AVoxelWorld::GetEnableMeshCompression() const
{
	return bEnableMeshCompression;
}

int AVoxelWorld::GetPositionQuantizationBits() const
{
	return PositionQuantizationBits;
}

int AVoxelWorld::GetNormalQuantizationBits() const
{
	return NormalQuantizationBits;
}

int AVoxelWorld::GetColorQuantizationBits() const
{
	return ColorQuantizationBits;
}

int AVoxelWorld::GetMeshCompressionLevel() const
{
	return MeshCompressionLevel;
}

float AVoxelWorld::GetNormalThresholdForSimplification() const
{
    return NormalThresholdForSimplification;
}

int AVoxelWorld::GetLOD() const
{
	return WorldLOD<0 ? Depth : FMath::Clamp<int>(WorldLOD, 0, Depth);
}

int AVoxelWorld::GetDepth() const
{
	return Depth;
}

int AVoxelWorld::GetMeshDepth() const
{
    return MeshDepth;
}

int AVoxelWorld::GetLowestProgressiveLOD() const
{
    return LowestProgressiveLOD;
}

bool AVoxelWorld::IsAutoUpdateMesh() const
{
	return bAutoUpdateMesh;
}

bool AVoxelWorld::IsProgressiveLODEnabled() const
{
	return bEnableProgressiveLOD;
}

bool AVoxelWorld::IsCachedMeshEnabled() const
{
	return bEnableCachedMesh;
}

bool AVoxelWorld::IsAsyncCollisionCookingEnabled() const
{
	return bUseAsyncCollisionCooking;
}

bool AVoxelWorld::IsBuildPNTesselationEnabled() const
{
	return bBuildPNTesselation;
}

FIntVector AVoxelWorld::GetMinimalCornerPosition() const
{
    check(Data.IsValid());
    return Data->GetMinimalCornerPosition();
}

FIntVector AVoxelWorld::GetMaximalCornerPosition() const
{
    check(Data.IsValid());
    return Data->GetMaximalCornerPosition();
}

FIntVector AVoxelWorld::GlobalToLocal(const FVector& Position) const
{
	FVector P = GetTransform().InverseTransformPosition(Position) / GetVoxelSize();
	return FIntVector(FMath::RoundToInt(P.X), FMath::RoundToInt(P.Y), FMath::RoundToInt(P.Z));
}

FVector AVoxelWorld::LocalToGlobal(const FIntVector& Position) const
{
	return GetTransform().TransformPosition(GetVoxelSize() * (FVector)Position);
}

TArray<FIntVector> AVoxelWorld::GetNeighboringPositions(const FVector& GlobalPosition) const
{
	FVector P = GetTransform().InverseTransformPosition(GlobalPosition) / GetVoxelSize();
	return TArray<FIntVector>({
		FIntVector(FMath::FloorToInt(P.X), FMath::FloorToInt(P.Y), FMath::FloorToInt(P.Z)),
		FIntVector(FMath::CeilToInt(P.X) , FMath::FloorToInt(P.Y), FMath::FloorToInt(P.Z)),
		FIntVector(FMath::FloorToInt(P.X), FMath::CeilToInt(P.Y) , FMath::FloorToInt(P.Z)),
		FIntVector(FMath::CeilToInt(P.X) , FMath::CeilToInt(P.Y) , FMath::FloorToInt(P.Z)),
		FIntVector(FMath::FloorToInt(P.X), FMath::FloorToInt(P.Y), FMath::CeilToInt(P.Z)),
		FIntVector(FMath::CeilToInt(P.X) , FMath::FloorToInt(P.Y), FMath::CeilToInt(P.Z)),
		FIntVector(FMath::FloorToInt(P.X), FMath::CeilToInt(P.Y) , FMath::CeilToInt(P.Z)),
		FIntVector(FMath::CeilToInt(P.X) , FMath::CeilToInt(P.Y) , FMath::CeilToInt(P.Z))
	});
}

int AVoxelWorld::GetDepthAt(const FIntVector& Position) const
{
	if (IsInWorld(Position))
	{
		return Render->GetDepthAt(Position);
	}
	else
	{
		UE_LOG(LogVoxel, Error, TEXT("GetDepthAt: Not in world: (%d, %d, %d)"), Position.X, Position.Y, Position.Z);
		return 0;
	}
}

bool AVoxelWorld::IsInWorld(const FIntVector& Position) const
{
	return Data->IsInWorld(Position.X, Position.Y, Position.Z);
}

float AVoxelWorld::GetVoxelSize() const
{
	return VoxelSize;
}

int AVoxelWorld::Size() const
{
	return Data->Size();
}

void AVoxelWorld::DrawDebugVoxelXY(int32 Z)
{
    check(IsCreated());

    UWorld* W = GetWorld();
    const FIntVector MinCorner = Data->GetMinimalCornerPosition();
    const FIntVector MaxCorner = Data->GetMaximalCornerPosition();
    const float VoxelSize = GetVoxelSize();

    if (Z<=MinCorner.Z && Z>=MaxCorner.Z)
    {
        return;
    }

    float v;
    FVoxelMaterial m;
    FColor c(0,0,0,255);

    for (int32 y=MinCorner.Y; y<MaxCorner.Y; ++y)
    {
        for (int32 x=MinCorner.X; x<MaxCorner.X; ++x)
        {
            FVector p(x*VoxelSize, y*VoxelSize, Z*VoxelSize);
            Data->GetValueAndMaterial(x, y, Z, v, m);

            if (v < 0)
            {
                c.R = -v*255;
                c.G = 0;
                if (v > -.999f)
                    DrawDebugSphere(W, p, 4, 4, c, true);
            }
            else
            {
                c.R = 0;
                c.G = v*255;
                if (v < .999f)
                    DrawDebugSphere(W, p, 4, 4, c, true);
            }
        }
    }
}

void AVoxelWorld::DrawDebugVoxelXZ(int32 Y)
{
    check(IsCreated());

    UWorld* W = GetWorld();
    const FIntVector MinCorner = Data->GetMinimalCornerPosition();
    const FIntVector MaxCorner = Data->GetMaximalCornerPosition();
    const float VoxelSize = GetVoxelSize();

    if (Y<=MinCorner.Y && Y>=MaxCorner.Y)
    {
        return;
    }

    float v;
    FVoxelMaterial m;
    FColor c(0,0,0,255);

    for (int32 z=MinCorner.Z; z<MaxCorner.Z; ++z)
    {
        for (int32 x=MinCorner.X; x<MaxCorner.X; ++x)
        {
            FVector p(x*VoxelSize, Y*VoxelSize, z*VoxelSize);
            Data->GetValueAndMaterial(x, Y, z, v, m);

            if (v < 0)
            {
                c.R = -v*255;
                c.G = 0;
                if (v > -.999f)
                    DrawDebugSphere(W, p, 4, 4, c, true);
            }
            else
            {
                c.R = 0;
                c.G = v*255;
                if (v < .999f)
                    DrawDebugSphere(W, p, 4, 4, c, true);
            }
        }
    }
}

void AVoxelWorld::DrawDebugVoxelYZ(int32 X)
{
    check(IsCreated());

    UWorld* W = GetWorld();
    const FIntVector MinCorner = Data->GetMinimalCornerPosition();
    const FIntVector MaxCorner = Data->GetMaximalCornerPosition();
    const float VoxelSize = GetVoxelSize();

    if (X<=MinCorner.X && X>=MaxCorner.X)
    {
        return;
    }

    float v;
    FVoxelMaterial m;
    FColor c(0,0,0,255);

    for (int32 z=MinCorner.Z; z<MaxCorner.Z; ++z)
    {
        for (int32 y=MinCorner.Y; y<MaxCorner.Y; ++y)
        {
            FVector p(X*VoxelSize, y*VoxelSize, z*VoxelSize);
            Data->GetValueAndMaterial(X, y, z, v, m);

            if (v < 0)
            {
                c.R = -v*255;
                c.G = 0;
                if (v > -.999f)
                    DrawDebugSphere(W, p, 4, 4, c, true);
            }
            else
            {
                c.R = 0;
                c.G = v*255;
                if (v < .999f)
                    DrawDebugSphere(W, p, 4, 4, c, true);
            }
        }
    }
}
