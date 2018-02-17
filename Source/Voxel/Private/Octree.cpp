// Copyright 2017 Phyronnaz

#include "Octree.h"

FOctree::FOctree(FIntVector Position, uint8 Depth, uint64 Id /*= -1*/) : Position(Position), Depth(Depth), Id(Id), bHasChilds(false)
{
	// Max for Id
	check(Depth <= 19);
}

bool FOctree::operator==(const FOctree& Other) const
{
	check((Id == Other.Id) == (Position == Other.Position && Depth == Other.Depth));
	return Id == Other.Id;
}

bool FOctree::operator<(const FOctree& Other) const
{
	return Id < Other.Id;
}

bool FOctree::operator>(const FOctree& Other) const
{
	return Id > Other.Id;
}

int FOctree::Size() const
{
	return 16 << Depth;
}

FIntVector FOctree::GetMinimalCornerPosition() const
{
	return Position - FIntVector(Size() / 2, Size() / 2, Size() / 2);
}

FIntVector FOctree::GetMaximalCornerPosition() const
{
	return Position + FIntVector(Size() / 2, Size() / 2, Size() / 2);
}

bool FOctree::IsLeaf() const
{
	return !bHasChilds;
}

bool FOctree::IsInOctree(int X, int Y, int Z) const
{
	return Position.X - Size() / 2 <= X && X < Position.X + Size() / 2
		&& Position.Y - Size() / 2 <= Y && Y < Position.Y + Size() / 2
		&& Position.Z - Size() / 2 <= Z && Z < Position.Z + Size() / 2;
}

void FOctree::LocalToGlobal(int X, int Y, int Z, int& OutX, int& OutY, int& OutZ) const
{
	OutX = X + (Position.X - Size() / 2);
	OutY = Y + (Position.Y - Size() / 2);
	OutZ = Z + (Position.Z - Size() / 2);
}

void FOctree::GlobalToLocal(int X, int Y, int Z, int& OutX, int& OutY, int& OutZ) const
{
	OutX = X - (Position.X - Size() / 2);
	OutY = Y - (Position.Y - Size() / 2);
	OutZ = Z - (Position.Z - Size() / 2);
}

uint64 FOctree::GetTopIdFromDepth(int8 Depth)
{
	return IntPow9(Depth);
}

void FOctree::GetIDsAt(uint64 ID, uint8 LOD, uint64 IDs[8])
{
	const uint64 Pow = IntPow9(LOD);
    IDs[0] = ID+1*Pow;
    IDs[1] = ID+2*Pow;
    IDs[2] = ID+3*Pow;
    IDs[3] = ID+4*Pow;
    IDs[4] = ID+5*Pow;
    IDs[5] = ID+6*Pow;
    IDs[6] = ID+7*Pow;
    IDs[7] = ID+8*Pow;
}

void FOctree::GetIDsAt(uint64 ID, uint8 LOD, TArray<uint64>& IDs)
{
	const uint64 Pow = IntPow9(LOD);
    IDs.Emplace(ID+1*Pow);
    IDs.Emplace(ID+2*Pow);
    IDs.Emplace(ID+3*Pow);
    IDs.Emplace(ID+4*Pow);
    IDs.Emplace(ID+5*Pow);
    IDs.Emplace(ID+6*Pow);
    IDs.Emplace(ID+7*Pow);
    IDs.Emplace(ID+8*Pow);
}

void FOctree::GetIDsAt(uint64 ID, uint8 Depth, uint8 EndDepth, TArray<uint64>& OutIDs)
{
    if (Depth > EndDepth)
    {
        // Next depth
        const uint8 LOD = Depth-1;

        if (LOD == EndDepth)
        {
            // At the specified depth, write result ids
            GetIDsAt(ID, LOD, OutIDs);
        }
        else
        {
            // Get child ids
            uint64 IDs[8];
            FOctree::GetIDsAt(ID, LOD, IDs);
            // Recursively gather ids
            GetIDsAt(IDs[0], LOD, EndDepth, OutIDs);
            GetIDsAt(IDs[1], LOD, EndDepth, OutIDs);
            GetIDsAt(IDs[2], LOD, EndDepth, OutIDs);
            GetIDsAt(IDs[3], LOD, EndDepth, OutIDs);
            GetIDsAt(IDs[4], LOD, EndDepth, OutIDs);
            GetIDsAt(IDs[5], LOD, EndDepth, OutIDs);
            GetIDsAt(IDs[6], LOD, EndDepth, OutIDs);
            GetIDsAt(IDs[7], LOD, EndDepth, OutIDs);
        }
    }
    // Specified start depth equals end depth, simply adds starting id as result
    else
    {
        OutIDs.Emplace(ID);
    }
}
