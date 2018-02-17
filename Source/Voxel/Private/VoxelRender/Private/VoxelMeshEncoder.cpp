// 

#include "VoxelMeshEncoder.h"
#include "DracoTypes.h"

DECLARE_CYCLE_STAT(TEXT("VoxelMeshEncoder ~ EncodeMeshSection"), STAT_EncodeMeshSection, STATGROUP_Voxel);
DECLARE_CYCLE_STAT(TEXT("VoxelMeshEncoder ~ DecodeMeshSection"), STAT_DecodeMeshSection, STATGROUP_Voxel);

FVoxelMeshEncoder::FVoxelMeshEncoder(int32 InPositionQuantizationBits, int32 InNormalQuantizationBits, int32 InColorQuantizationBits, int32 InCompressionLevel)
    : PositionQuantizationBits(InPositionQuantizationBits)
    , NormalQuantizationBits(InNormalQuantizationBits)
    , ColorQuantizationBits(InColorQuantizationBits)
    , CompressionSpeed(FMath::Clamp(10-InCompressionLevel, 0, 10))
{
}

void FVoxelMeshEncoder::EncodeMeshSection(const FVoxelProcMeshSection& Section, TArray<uint8>& ByteData)
{
	SCOPE_CYCLE_COUNTER(STAT_EncodeMeshSection);

    const TArray<FVoxelProcMeshVertex>& Vertices( Section.ProcVertexBuffer );
    const TArray<int32>& Indices( Section.ProcIndexBuffer );

    const int32 vCount = Vertices.Num();
    const int32 iCount = Indices.Num();
    const int32 tCount = iCount/3;

    using namespace draco;

    Mesh mesh;
    
    mesh.SetNumFaces(tCount);
    mesh.set_num_points(iCount);

    PointAttribute* posAtt = nullptr;
    PointAttribute* nrmAtt = nullptr;
    PointAttribute* clrAtt = nullptr;

    // Construct position attribute
    {
        GeometryAttribute va;
        va.Init(GeometryAttribute::POSITION, nullptr, 3, DT_FLOAT32,
                false, sizeof(float) * 3, 0);
        const int32 attId = mesh.AddAttribute(va, false, vCount);
        posAtt = mesh.attribute(attId);
    }

    // Construct normal attribute
    {
        GeometryAttribute va;
        va.Init(GeometryAttribute::NORMAL, nullptr, 3, DT_FLOAT32,
                false, sizeof(float) * 3, 0);
        const int32 attId = mesh.AddAttribute(va, false, vCount);
        nrmAtt = mesh.attribute(attId);
    }

    // Construct color attribute
    {
        GeometryAttribute va;
        va.Init(GeometryAttribute::COLOR, nullptr, 4, DT_UINT8,
                false, 4, 0);
        const int32 attId = mesh.AddAttribute(va, false, vCount);
        clrAtt = mesh.attribute(attId);
    }

    // Make sure attributes are valid
    check(posAtt && nrmAtt && clrAtt);

    // Construct attribute values
    {
        float pos[3];
        float nrm[3];
        uint8 clr[4];

        for (AttributeValueIndex avi(0); avi<vCount; ++avi)
        {
            const FVoxelProcMeshVertex& vertex( Vertices[avi.value()] );
            const FVector& posRef( vertex.Position );
            const FVector& nrmRef( vertex.Normal );
            const FColor& clrRef( vertex.Color );

            pos[0] = posRef.X;
            pos[1] = posRef.Y;
            pos[2] = posRef.Z;

            nrm[0] = nrmRef.X;
            nrm[1] = nrmRef.Y;
            nrm[2] = nrmRef.Z;

            clr[0] = clrRef.R;
            clr[1] = clrRef.G;
            clr[2] = clrRef.B;
            clr[3] = clrRef.A;

            posAtt->SetAttributeValue(avi, pos);
            nrmAtt->SetAttributeValue(avi, nrm);
            clrAtt->SetAttributeValue(avi, clr);
        }
    }

    // Construct indices
    for (int32 i=0; i<iCount; ++i)
    {
        const PointIndex vid(i);
        const AttributeValueIndex iid(Indices[i]);
        posAtt->SetPointMapEntry(vid, iid);
        nrmAtt->SetPointMapEntry(vid, iid);
        clrAtt->SetPointMapEntry(vid, iid);
    }

    // Construct mesh faces
    {
        Mesh::Face face;
        for (FaceIndex fi(0); fi<tCount; ++fi)
        {
            // Map mesh face with inverse ordered indices
            for (int32 c=0; c<3; ++c)
                face[2-c] = fi.value() * 3 + c;
            mesh.SetFace(fi, face);
        }
    }

    // Deduplicate values and ids
    mesh.DeduplicateAttributeValues();
    mesh.DeduplicatePointIds();

    Encoder encoder;
    encoder.SetAttributeQuantization(GeometryAttribute::POSITION, PositionQuantizationBits);
    encoder.SetAttributeQuantization(GeometryAttribute::NORMAL, NormalQuantizationBits);
    encoder.SetAttributeQuantization(GeometryAttribute::COLOR, ColorQuantizationBits);
    encoder.SetSpeedOptions(CompressionSpeed, CompressionSpeed);

    EncoderBuffer buffer;
    const Status status = encoder.EncodeMeshToBuffer(mesh, &buffer);

    if (status.ok())
    {
        const void* rawData = buffer.data();
        const uint8* bufferData = reinterpret_cast<const uint8*>(rawData);
        const SIZE_T bufferSize = buffer.size();

        ByteData.SetNumUninitialized(bufferSize);
        FMemory::Memcpy(ByteData.GetData(), bufferData, bufferSize);
    }
}

void FVoxelMeshEncoder::DecodeMeshSection(const TArray<uint8>& ByteData, FVoxelProcMeshSection& Section)
{
	SCOPE_CYCLE_COUNTER(STAT_DecodeMeshSection);

    using namespace draco;

    // Create a draco decoding buffer
    const char* bufferData = reinterpret_cast<const char*>( ByteData.GetData() );
    const SIZE_T bufferSize = ByteData.Num();

    DecoderBuffer buffer;
    buffer.Init(bufferData, bufferSize);

    // Get encoded geometry type from buffer data
    StatusOr<EncodedGeometryType> encoding_type_statusor = Decoder::GetEncodedGeometryType(&buffer);

    if (! encoding_type_statusor.ok())
    {
        UE_LOG(LogTemp,Warning,
            TEXT("DECODE FAILED - %s"),
            *FString(encoding_type_statusor.status().error_msg()));
        return;
    }

    const EncodedGeometryType geom_type( encoding_type_statusor.value() );
    Mesh mesh;

    // Decode mesh if buffer encoding type matches
    if (geom_type == TRIANGULAR_MESH)
    {
        Decoder decoder;
        Status decode_status = decoder.DecodeBufferToGeometry(&buffer, &mesh);

        if (! decode_status.ok())
        {
            UE_LOG(LogTemp,Warning,
                TEXT("DECODE FAILED - %s"),
                *FString(decode_status.error_msg()));
            return;
        }
    }
    else
    {
        UE_LOG(LogTemp,Warning,
            TEXT("DECODE ABORTED - Failed to decode mesh"));
        return;
    }

    // Construct Mesh Geometry

    typedef FVoxelProcMeshVertex FVertex;
    typedef TTuple<int32, int32, int32> FIndexTuple;

    TArray<FVertex>& Vertices( Section.ProcVertexBuffer );
    TArray<int32>& Indices( Section.ProcIndexBuffer );
    TMap<const FIndexTuple, int32> IndexMapping;

    Indices.SetNumZeroed(mesh.num_faces()*3);
    Vertices.Reserve(Indices.Num());
    IndexMapping.Reserve(Indices.Num());

    const PointAttribute* posAtt = mesh.GetNamedAttribute(GeometryAttribute::POSITION);
    const PointAttribute* nrmAtt = mesh.GetNamedAttribute(GeometryAttribute::NORMAL);
    const PointAttribute* clrAtt = mesh.GetNamedAttribute(GeometryAttribute::COLOR);

    std::array<float, 3> f3;
    std::array<uint8, 4> u4;

    check(posAtt && nrmAtt && clrAtt);

    // Reconstruct mesh geometry
    for (int32 i=0; i<mesh.num_faces(); ++i)
    {
        const Mesh::Face& face( mesh.face(FaceIndex(i)) );

        const int32 pi0( posAtt->mapped_index(face[0]).value() );
        const int32 pi1( posAtt->mapped_index(face[1]).value() );
        const int32 pi2( posAtt->mapped_index(face[2]).value() );

        const int32 ni0( nrmAtt->mapped_index(face[0]).value() );
        const int32 ni1( nrmAtt->mapped_index(face[1]).value() );
        const int32 ni2( nrmAtt->mapped_index(face[2]).value() );

        const int32 ci0( clrAtt->mapped_index(face[0]).value() );
        const int32 ci1( clrAtt->mapped_index(face[1]).value() );
        const int32 ci2( clrAtt->mapped_index(face[2]).value() );

        const FIndexTuple i0( MakeTuple(pi0, ni0, ci0) );
        const FIndexTuple i1( MakeTuple(pi1, ni1, ci1) );
        const FIndexTuple i2( MakeTuple(pi2, ni2, ci2) );

        int32* k0 = IndexMapping.Find(i0);
        int32* k1 = IndexMapping.Find(i1);
        int32* k2 = IndexMapping.Find(i2);

        if (! k0)
        {
            AttributeValueIndex pavi(pi0);
            AttributeValueIndex cavi(ci0);
            FVertex vertex;
            FVector& pos( vertex.Position );

            posAtt->GetValue<float, 3>(pavi, &f3);
            {
                pos.Set(f3[0], f3[1], f3[2]);
            }
            nrmAtt->GetValue<float, 3>(pavi, &f3);
            {
                vertex.Normal.Set(f3[0], f3[1], f3[2]);
            }
            clrAtt->GetValue<uint8, 4>(cavi, &u4);
            {
                vertex.Color = FColor(u4[0], u4[1], u4[2], u4[3]);
            }

            k0 = &IndexMapping.Emplace(i0, Vertices.Emplace(vertex));
        }
        if (! k1)
        {
            AttributeValueIndex pavi(pi1);
            AttributeValueIndex cavi(ci1);
            FVertex vertex;
            FVector& pos( vertex.Position );

            posAtt->GetValue<float, 3>(pavi, &f3);
            {
                pos.Set(f3[0], f3[1], f3[2]);
            }
            nrmAtt->GetValue<float, 3>(pavi, &f3);
            {
                vertex.Normal.Set(f3[0], f3[1], f3[2]);
            }
            clrAtt->GetValue<uint8, 4>(cavi, &u4);
            {
                vertex.Color = FColor(u4[0], u4[1], u4[2], u4[3]);
            }

            k1 = &IndexMapping.Emplace(i1, Vertices.Emplace(vertex));
        }
        if (! k2)
        {
            AttributeValueIndex pavi(pi2);
            AttributeValueIndex cavi(ci2);
            FVertex vertex;
            FVector& pos( vertex.Position );

            posAtt->GetValue<float, 3>(pavi, &f3);
            {
                pos.Set(f3[0], f3[1], f3[2]);
            }
            nrmAtt->GetValue<float, 3>(pavi, &f3);
            {
                vertex.Normal.Set(f3[0], f3[1], f3[2]);
            }
            clrAtt->GetValue<uint8, 4>(cavi, &u4);
            {
                vertex.Color = FColor(u4[0], u4[1], u4[2], u4[3]);
            }

            k2 = &IndexMapping.Emplace(i2, Vertices.Emplace(vertex));
        }

        int32 ti = i * 3;
        Indices[ti  ] = *k2;
        Indices[ti+1] = *k1;
        Indices[ti+2] = *k0;
    }

    // Clears mapping continer
    IndexMapping.Empty();

    // Shrink containers
    Vertices.Shrink();
    Indices.Shrink();
}
