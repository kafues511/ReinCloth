// Copyright © 2026 kafues511 All Rights Reserved.

#include "ReinClothDataAsset.h"
#include "Engine/SkeletalMesh.h"

#if WITH_EDITOR
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshRenderData.h"

#include "Algo/IndexOf.h"
#include "Algo/AllOf.h"

#include "StaticMeshAttributes.h"

#include "ReinClothLib.h"
#endif  // WITH_EDITOR

#define LOCTEXT_NAMESPACE "ReinClothDataAsset"

// UE版
#define REIN_CLOTH_SETUP_MODE_NATIVE (0)
// Lib版
#define REIN_CLOTH_SETUP_MODE_LIB (1)
// UE版とLib版の結果が同じか比較するモード
#define REIN_CLOTH_SETUP_MODE_COMPARE (2)

#ifndef REIN_CLOTH_SETUP_MODE
#define REIN_CLOTH_SETUP_MODE REIN_CLOTH_SETUP_MODE_LIB
#endif

#if WITH_EDITOR
namespace ReinCloth
{
	template<typename UVType>
	struct TUniqueVertex
	{
		FVector3f Position;
		FVector3f Normal;
		UVType UV;
		FColor Color;
		FLinearColor LinearColor;
		FUintVector4 Bones;
		FUintVector4 ExtraBones;
		FVector4f Weights;
		FVector4f ExtraWeights;
	};

	template<typename UVType>
	static bool Setup(
		const USkeletalMesh* SkeletalMeshAsset,
		int32 SectionIndex,
		int32 UVChannel,
		float GridSize,
		int32& OutBaseVertex,
		TArray<ReinCloth::TUniqueVertex<UVType>>& OutVertices,
		TArray<uint32>& OutIndices,
		TArray<FUintVector4>& OutTriangles)
	{
		auto SkeletalMeshModel = SkeletalMeshAsset->GetImportedModel();
		if (SkeletalMeshModel == nullptr)
		{
			return false;
		}

		auto RenderData = SkeletalMeshAsset->GetResourceForRendering();
		if (RenderData == nullptr)
		{
			return false;
		}

		const int32 LODIndex = 0;
		if (!RenderData->LODRenderData.IsValidIndex(LODIndex) || !SkeletalMeshModel->LODModels.IsValidIndex(LODIndex))
		{
			return false;
		}

		const auto& LODRenderData = RenderData->LODRenderData[LODIndex];
		const auto& LODModelRef = SkeletalMeshModel->LODModels[LODIndex];

		if (!LODModelRef.Sections.IsValidIndex(SectionIndex))
		{
			return false;
		}

		const auto& SkelMeshSection = LODModelRef.Sections[SectionIndex];
		const auto& SoftVertices = SkelMeshSection.SoftVertices;

		OutBaseVertex = 0;
		for (int32 Index = 0; Index < SectionIndex; ++Index)
		{
			OutBaseVertex += LODModelRef.Sections[Index].NumVertices;
		}

		OutVertices.Empty();
		OutIndices.Empty();
		for (int32 Index = 0; Index < SoftVertices.Num(); ++Index)
		{
			const auto& Vertex = SoftVertices[Index];

			ReinCloth::TUniqueVertex<UVType> NewVertex;
			NewVertex.Position = Vertex.Position;
			NewVertex.Normal = Vertex.TangentZ;
			if constexpr (std::is_same_v<UVType, FVector2f>)
			{
				NewVertex.UV.X = Vertex.UVs[UVChannel].X;
				NewVertex.UV.Y = Vertex.UVs[UVChannel].Y;
			}
			else if constexpr (std::is_same_v<UVType, FUintVector2>)
			{
				NewVertex.UV.X = FMath::CeilToInt(Vertex.UVs[UVChannel].X * GridSize);
				NewVertex.UV.Y = FMath::CeilToInt(Vertex.UVs[UVChannel].Y * GridSize);
			}
			else
			{
				return false;
			}
			NewVertex.Color = Vertex.Color;
			NewVertex.LinearColor = Vertex.Color.ReinterpretAsLinear();
			for (int32 Layer = 0; Layer < 4; ++Layer)
			{
				NewVertex.Bones[Layer] = SkelMeshSection.BoneMap[Vertex.InfluenceBones[Layer]];
				NewVertex.Weights[Layer] = static_cast<float>(Vertex.InfluenceWeights[Layer]) / 65535.0f;
			}
			for (int32 ExtraLayer = 0; ExtraLayer < 4; ++ExtraLayer)
			{
				NewVertex.ExtraBones[ExtraLayer] = SkelMeshSection.BoneMap[Vertex.InfluenceBones[4 + ExtraLayer]];
				NewVertex.ExtraWeights[ExtraLayer] = static_cast<float>(Vertex.InfluenceWeights[4 + ExtraLayer]) / 65535.0f;
			}

			auto NewVertexIndex = Algo::IndexOfByPredicate(OutVertices, [UV = NewVertex.UV](const auto& Element) { return Element.UV == UV; });
			if (NewVertexIndex == INDEX_NONE)
			{
				NewVertexIndex = OutVertices.Add(NewVertex);
			}
			OutIndices.Add(NewVertexIndex);
		}

		auto IndexBuffer = LODRenderData.MultiSizeIndexContainer.GetIndexBuffer();

		OutTriangles.Empty();
		OutTriangles.Reserve(SkelMeshSection.NumTriangles);
		for (uint32 TriangleIndex = 0u; TriangleIndex < SkelMeshSection.NumTriangles; ++TriangleIndex)
		{
			auto NewTriangle = FUintVector4::ZeroValue;
			for (uint32 PointIndex = 0u; PointIndex < 3u; ++PointIndex)
			{
				uint32 IndexBufferValue = IndexBuffer->Get(SkelMeshSection.BaseIndex + ((TriangleIndex * 3) + PointIndex));
				NewTriangle[PointIndex] = OutIndices[IndexBufferValue - OutBaseVertex];
			}
			OutTriangles.Add(NewTriangle);
		}

		return true;
	}

	struct FReinClothPieceImpl : public FReinClothPiece
	{
		TArray<ReinCloth::TUniqueVertex<FVector2f>> Vertices;
		TArray<uint32> Indices;
		TArray<FUintVector4> Triangles;
		int32 BaseVertex;
	};

	/**
	 * @brief スロット名からセクションインデックスを探す
	 * @param SkeletalMeshAsset 対象のSkeletalMesh
	 * @param SlotName スロット名
	 * @return 見つからない場合は INDEX_NONE を返します。
	 */
	static int32 FindSectionIndexBySlotName(const USkeletalMesh* SkeletalMeshAsset, FName SlotName)
	{
		if (!IsValid(SkeletalMeshAsset))
		{
			// ソースがない
			return INDEX_NONE;
		}

		if (SlotName == NAME_None)
		{
			// 不正なスロット名
			return INDEX_NONE;
		}

		auto RenderData = SkeletalMeshAsset->GetResourceForRendering();
		if (RenderData == nullptr)
		{
			return INDEX_NONE;
		}

		const int32 LODIndex = 0;
		if (!RenderData->LODRenderData.IsValidIndex(LODIndex))
		{
			return INDEX_NONE;
		}

		const auto& LODData = RenderData->LODRenderData[LODIndex];
		const auto& Materials = SkeletalMeshAsset->GetMaterials();

		int32 TargetMaterialIndex = INDEX_NONE;
		for (int32 MaterialIndex = 0; MaterialIndex < Materials.Num(); ++MaterialIndex)
		{
			if (Materials[MaterialIndex].MaterialSlotName == SlotName)
			{
				TargetMaterialIndex = MaterialIndex;
				break;
			}
		}

		if (TargetMaterialIndex == INDEX_NONE)
		{
			// スロット名が見つからない
			return INDEX_NONE;
		}

		for (int32 SectionIndex = 0; SectionIndex < LODData.RenderSections.Num(); ++SectionIndex)
		{
			if (LODData.RenderSections[SectionIndex].MaterialIndex == TargetMaterialIndex)
			{
				return SectionIndex;
			}
		}

		return INDEX_NONE;
	}
};

namespace ReinClothLib
{
	template<typename UVType>
	static void BuildLibVertices(const TArray<ReinCloth::TUniqueVertex<UVType>>& InVertices, TArray<ReinCloth_Vertex>& OutVertices)
	{
		OutVertices.Empty();
		OutVertices.Reserve(InVertices.Num());
		for (const auto& Src : InVertices)
		{
			ReinCloth_Vertex Dst{};
			Dst.Position = { Src.Position.X, Src.Position.Y, Src.Position.Z };
			Dst.Normal = { Src.Normal.X, Src.Normal.Y, Src.Normal.Z };
			Dst.UV = { static_cast<float>(Src.UV[0]), static_cast<float>(Src.UV[1]) };
			Dst.Color = { Src.Color.B, Src.Color.G, Src.Color.R, Src.Color.A };
			Dst.LinearColor = { Src.LinearColor.R, Src.LinearColor.G, Src.LinearColor.B, Src.LinearColor.A };
			Dst.Bones = { Src.Bones[0], Src.Bones[1], Src.Bones[2], Src.Bones[3] };
			Dst.ExtraBones = { Src.ExtraBones[0], Src.ExtraBones[1], Src.ExtraBones[2], Src.ExtraBones[3] };
			Dst.Weights = { Src.Weights[0], Src.Weights[1], Src.Weights[2], Src.Weights[3] };
			Dst.ExtraWeights = { Src.ExtraWeights[0], Src.ExtraWeights[1], Src.ExtraWeights[2], Src.ExtraWeights[3] };
			OutVertices.Add(Dst);
		}
	}

	static bool SetBuildInput(
		void* Builder,
		const TArray<ReinCloth::FReinClothPieceImpl>& BuildPieces,
		const TArray<ReinCloth::TUniqueVertex<FUintVector2>>& RenderMeshVertices,
		const TArray<FUintVector4>& RenderMeshTriangles,
		bool bIsWrap,
		FString& OutMessage)
	{
		for (const auto& Piece : BuildPieces)
		{
			TArray<ReinCloth_Vertex> LibVertices;
			BuildLibVertices(Piece.Vertices, LibVertices);
			if (ReinCloth_AddSimulationPiece(
				Builder,
				static_cast<int32>(Piece.Type),
				LibVertices.Num(),
				LibVertices.GetData(),
				Piece.Indices.Num(),
				Piece.Indices.GetData(),
				Piece.Triangles.Num(),
				Piece.Triangles.GetData(),
				bIsWrap ? 1 : 0) == 0)
			{
				OutMessage = TEXT("ReinClothLibへのシミュレーションパーツ登録に失敗");
				return false;
			}
		}

		TArray<ReinCloth_Vertex> LibRenderMeshVertices;
		BuildLibVertices(RenderMeshVertices, LibRenderMeshVertices);
		if (ReinCloth_SetRenderMesh(
			Builder,
			LibRenderMeshVertices.Num(),
			LibRenderMeshVertices.GetData(),
			RenderMeshTriangles.Num(),
			RenderMeshTriangles.GetData()) == 0)
		{
			OutMessage = TEXT("ReinClothLibへのレンダーメッシュ登録に失敗");
			return false;
		}

		return true;
	}

	struct FBuilderScope
	{
		void* Builder = nullptr;

		~FBuilderScope()
		{
			ReinCloth_DestroyBuilder(Builder);
		}
	};

	static bool CreateBuilder(FBuilderScope& OutBuilder, FString& OutMessage)
	{
		OutBuilder.Builder = ReinCloth_CreateBuilder();
		if (OutBuilder.Builder == nullptr)
		{
			OutMessage = TEXT("ReinClothLibのBuilder作成に失敗");
			return false;
		}

		return true;
	}

	template<typename ElementType>
	static bool CopyLibResult(void* Builder, int32 BufferType, TArray<ElementType>& OutArray, FString& OutMessage)
	{
		int32 NumElements = 0;
		int32 ElementSize = 0;
		if (ReinCloth_GetResultDesc(Builder, BufferType, &NumElements, &ElementSize) == 0)
		{
			OutMessage = FString::Format(TEXT("ReinClothLibの結果情報取得に失敗 BufferType={0}"), { BufferType });
			return false;
		}

		if (ElementSize != sizeof(ElementType))
		{
			OutMessage = FString::Format(TEXT("ReinClothLibの要素サイズが一致しません BufferType={0}, Lib={1}, UE={2}"), { BufferType, ElementSize, sizeof(ElementType) });
			return false;
		}

		OutArray.SetNumUninitialized(NumElements);

		const int32 NumBytes = NumElements * ElementSize;
		if (NumBytes > 0 && ReinCloth_CopyResult(Builder, BufferType, OutArray.GetData(), NumBytes) == 0)
		{
			OutMessage = FString::Format(TEXT("ReinClothLibの結果コピーに失敗 BufferType={0}"), { BufferType });
			return false;
		}

		return true;
	}
};
#endif  // WITH_EDITOR

EReinClothSetupFlags FReinClothSimMeshSection::Setup(const USkeletalMesh* SkeletalRenderMeshAsset, const USkeletalMesh* SkeletalSimulationMeshAsset, int32 ClothUVChannel, float InGridSize, bool bIsForce, FString& OutMessage)
{
#if WITH_EDITOR
	const auto SkeletalRenderMeshModel = SkeletalRenderMeshAsset->GetImportedModel();
	if (SkeletalRenderMeshModel == nullptr)
	{
		OutMessage = TEXT("SkeletalRenderMeshAssetのImportedModelの取得に失敗");
		return EReinClothSetupFlags::Failed;
	}

	const auto SkeletalSimulationMeshModel = SkeletalSimulationMeshAsset->GetImportedModel();
	if (SkeletalSimulationMeshModel == nullptr)
	{
		OutMessage = TEXT("SkeletalSimulationMeshAssetのImportedModelの取得に失敗");
		return EReinClothSetupFlags::Failed;
	}

	if (Pieces.IsEmpty())
	{
		OutMessage = TEXT("シミュレーションパーツが設定されていません");
		return EReinClothSetupFlags::Failed;
	}

	FSHA1 SHA;
	SHA.Update(reinterpret_cast<const uint8*>(&SkeletalRenderMeshModel->SkeletalMeshModelGUID), sizeof(FGuid));
	SHA.Update(reinterpret_cast<const uint8*>(&SkeletalSimulationMeshModel->SkeletalMeshModelGUID), sizeof(FGuid));
	SHA.Update(reinterpret_cast<const uint8*>(&ClothUVChannel), sizeof(int32));
	SHA.Update(reinterpret_cast<const uint8*>(&InGridSize), sizeof(float));
	SHA.Update(reinterpret_cast<const uint8*>(&RenderSection), sizeof(int32));
	SHA.Update(reinterpret_cast<const uint8*>(&TargetInfluence), sizeof(EReinClothTargetInfluence));
	SHA.Update(reinterpret_cast<const uint8*>(&EmbeddedOffsetMode), sizeof(EReinClothEmbeddedOffsetMode));
	SHA.Update(reinterpret_cast<const uint8*>(&bIsWrap), sizeof(bool));
	for (const auto& Piece : Pieces)
	{
		SHA.Update(reinterpret_cast<const uint8*>(&Piece.SimulationSection), sizeof(int32));
		SHA.Update(reinterpret_cast<const uint8*>(&Piece.Type), sizeof(FReinClothPieceTypes));
	}
	SHA.Final();

	FSHAHash SHAHash;
	SHA.GetHash(&SHAHash.Hash[0]);
	FString NewGuid = SHAHash.ToString();
	if (Guid == NewGuid && !bIsForce)
	{
		return EReinClothSetupFlags::Already;
	}

	FScopedSlowTask SlowTask(0.0f, LOCTEXT("SetupCloth", "Setup Cloth ..."));
	SlowTask.MakeDialog();

	bIsValid = false;

#if REIN_CLOTH_SETUP_MODE == REIN_CLOTH_SETUP_MODE_NATIVE || REIN_CLOTH_SETUP_MODE == REIN_CLOTH_SETUP_MODE_COMPARE
	#error Unsupported REIN_CLOTH_SETUP_MODE
#endif  // REIN_CLOTH_SETUP_MODE == REIN_CLOTH_SETUP_MODE_NATIVE || REIN_CLOTH_SETUP_MODE == REIN_CLOTH_SETUP_MODE_COMPARE

#if REIN_CLOTH_SETUP_MODE == REIN_CLOTH_SETUP_MODE_LIB
	TArray<ReinCloth::FReinClothPieceImpl> BuildPieces(Pieces);

	// 制約座標が格納されているUVチャンネル
	const int32 ConstraintUVChannel = 0;

	for (auto& Piece : BuildPieces)
	{
		if (!ReinCloth::Setup(
			SkeletalSimulationMeshAsset,
			Piece.SimulationSection,
			ConstraintUVChannel,
			InGridSize,
			Piece.BaseVertex,
			Piece.Vertices,
			Piece.Indices,
			Piece.Triangles))
		{
			OutMessage = TEXT("シミュレーションメッシュの入力データ作成に失敗");
			return EReinClothSetupFlags::Failed;
		}
	}

	TArray<ReinCloth::TUniqueVertex<FUintVector2>> RenderMesh_Vertices;
	TArray<uint32> RenderMesh_Indices;
	TArray<FUintVector4> RenderMesh_Triangles;
	if (!ReinCloth::Setup(
		SkeletalRenderMeshAsset,
		RenderSection,
		ClothUVChannel,
		InGridSize,
		BaseVertex,
		RenderMesh_Vertices,
		RenderMesh_Indices,
		RenderMesh_Triangles))
	{
		OutMessage = TEXT("レンダーメッシュの入力データ作成に失敗");
		return EReinClothSetupFlags::Failed;
	}
#endif  // REIN_CLOTH_SETUP_MODE == REIN_CLOTH_SETUP_MODE_LIB

#if REIN_CLOTH_SETUP_MODE == REIN_CLOTH_SETUP_MODE_LIB || REIN_CLOTH_SETUP_MODE == REIN_CLOTH_SETUP_MODE_COMPARE
	ReinClothLib::FBuilderScope BuilderScope;
	if (!ReinClothLib::CreateBuilder(BuilderScope, OutMessage))
	{
		return EReinClothSetupFlags::Failed;
	}
	void* Builder = BuilderScope.Builder;

	if (!ReinClothLib::SetBuildInput(
		Builder,
		BuildPieces,
		RenderMesh_Vertices,
		RenderMesh_Triangles,
		bIsWrap,
		OutMessage))
	{
		return EReinClothSetupFlags::Failed;
	}

	if (ReinCloth_Build(Builder, static_cast<int32>(TargetInfluence), static_cast<int32>(EmbeddedOffsetMode)) == 0)
	{
		OutMessage = TEXT("ReinClothLibのBuildに失敗");
		return EReinClothSetupFlags::Failed;
	}

#if REIN_CLOTH_SETUP_MODE == REIN_CLOTH_SETUP_MODE_LIB
	if (!ReinClothLib::CopyLibResult(Builder, ReinCloth_Buffer_SimTriangles, SimTriangles, OutMessage) ||
		!ReinClothLib::CopyLibResult(Builder, ReinCloth_Buffer_Positions, Positions, OutMessage) ||
		!ReinClothLib::CopyLibResult(Builder, ReinCloth_Buffer_Velocities, Velocities, OutMessage) ||
		!ReinClothLib::CopyLibResult(Builder, ReinCloth_Buffer_Predicts, Predicts, OutMessage) ||
		!ReinClothLib::CopyLibResult(Builder, ReinCloth_Buffer_Normals, Normals, OutMessage) ||
		!ReinClothLib::CopyLibResult(Builder, ReinCloth_Buffer_Origins, Origins, OutMessage) ||
		!ReinClothLib::CopyLibResult(Builder, ReinCloth_Buffer_Influences, Influences, OutMessage) ||
		!ReinClothLib::CopyLibResult(Builder, ReinCloth_Buffer_Embeddeds, Embeddeds, OutMessage) ||
		!ReinClothLib::CopyLibResult(Builder, ReinCloth_Buffer_EmbeddedInfluences, EmbeddedInfluences, OutMessage) ||
		!ReinClothLib::CopyLibResult(Builder, ReinCloth_Buffer_RenderTriangles, RenderTriangles, OutMessage) ||
		!ReinClothLib::CopyLibResult(Builder, ReinCloth_Buffer_Constraints, Constraints, OutMessage) ||
		!ReinClothLib::CopyLibResult(Builder, ReinCloth_Buffer_Offsets, Offsets, OutMessage) ||
		!ReinClothLib::CopyLibResult(Builder, ReinCloth_Buffer_Neighbors, Neighbors, OutMessage) ||
		!ReinClothLib::CopyLibResult(Builder, ReinCloth_Buffer_NormalOffsets, NormalOffsets, OutMessage) ||
		!ReinClothLib::CopyLibResult(Builder, ReinCloth_Buffer_NormalNeighbors, NormalNeighbors, OutMessage))
	{
		return EReinClothSetupFlags::Failed;
	}
#elif REIN_CLOTH_SETUP_MODE == REIN_CLOTH_SETUP_MODE_COMPARE
	#error REIN_CLOTH_SETUP_MODE
#endif
#endif  // REIN_CLOTH_SETUP_MODE == REIN_CLOTH_SETUP_MODE_LIB || REIN_CLOTH_SETUP_MODE == REIN_CLOTH_SETUP_MODE_COMPARE

	NumPositions = Positions.Num();
	if (NumPositions >= 1024)
	{
		// 不正な頂点数
		return EReinClothSetupFlags::Failed;
	}

	if (Neighbors.Num() == 0 || Constraints.Num() == 0)
	{
		// ClothMeshのUVが格子状になっていないと思う
		return EReinClothSetupFlags::Failed;
	}

	bIsValid = true;

	Guid = NewGuid;

	return EReinClothSetupFlags::Newly;

#if REIN_CLOTH_SETUP_MODE != REIN_CLOTH_SETUP_MODE_NATIVE && REIN_CLOTH_SETUP_MODE != REIN_CLOTH_SETUP_MODE_LIB && REIN_CLOTH_SETUP_MODE != REIN_CLOTH_SETUP_MODE_COMPARE
	#error Unsupported REIN_CLOTH_SETUP_MODE
#endif
#else    // WITH_EDITOR
	return EReinClothSetupFlags::Already;
#endif  // WITH_EDITOR
}

EReinClothSetupFlags UReinClothDataAsset::SetupClothUV(bool bIsForce, FString& OutMessage)
{
#if WITH_EDITOR
	const auto SkeletalMeshModel = SkeletalRenderMeshAsset->GetImportedModel();
	if (SkeletalMeshModel == nullptr)
	{
		OutMessage = TEXT("SkeletalRenderMeshAssetのImportedModelの取得に失敗");
		return EReinClothSetupFlags::Failed;
	}

	const auto& Materials = SkeletalRenderMeshAsset->GetMaterials();

	FSHA1 SHA;
	SHA.Update(reinterpret_cast<const uint8*>(&SkeletalMeshModel->SkeletalMeshModelGUID), sizeof(FGuid));
	SHA.Update(reinterpret_cast<const uint8*>(&ClothUVChannel), sizeof(int32));
	for (const auto& Section : Sections)
	{
		// FNameはエンジン起動毎に内部テーブルが異なるためハッシュが一致しない
		auto NameStr = Section.RenderSlotName.ToString();
		auto NameBytes = NameStr.Len() * sizeof(TCHAR);
		SHA.Update(reinterpret_cast<const uint8*>(*NameStr), NameBytes);
	}
	SHA.Final();

	FSHAHash SHAHash;
	SHA.GetHash(&SHAHash.Hash[0]);
	auto NewGuid = SHAHash.ToString();
	auto bHasSectionGridSize = Algo::AllOf(Sections, [](const auto& Section) { return Section.GridSize > 0; });
	if (Guid == NewGuid && bHasSectionGridSize && !bIsForce)
	{
		return EReinClothSetupFlags::Already;
	}

	FScopedSlowTask SlowTask(0.0f, LOCTEXT("SetupClothUV", "Setup Cloth UV ..."));
	SlowTask.MakeDialog();

	const int32 LODIndex = 0;
	auto MeshDescription = SkeletalRenderMeshAsset->GetMeshDescription(LODIndex);
	if (MeshDescription == nullptr)
	{
		OutMessage = TEXT("SkeletalRenderMeshAssetのMeshDescriptionの取得に失敗");
		return EReinClothSetupFlags::Failed;
	}

	FStaticMeshAttributes MeshAttributes(*MeshDescription);
	auto VertexInstanceUVs = MeshAttributes.GetVertexInstanceUVs();
	auto PolygonGroupMaterialSlotNames = MeshAttributes.GetPolygonGroupMaterialSlotNames();

	if (VertexInstanceUVs.GetNumChannels() <= ClothUVChannel)
	{
		VertexInstanceUVs.SetNumChannels(ClothUVChannel + 1);
	}

	for (auto& Section : Sections)
	{
		FName ImportedMaterialSlotName = NAME_None;
		for (int32 MaterialIndex = 0; MaterialIndex < Materials.Num(); ++MaterialIndex)
		{
			if (Materials[MaterialIndex].MaterialSlotName == Section.RenderSlotName)
			{
				ImportedMaterialSlotName = Materials[MaterialIndex].ImportedMaterialSlotName;
				break;
			}
		}

		if (ImportedMaterialSlotName.IsNone())
		{
			OutMessage = FString::Format(TEXT("Render Slot Name {0} に対応するMaterialSlotが見つかりません。"), { Section.RenderSlotName.ToString() });
			return EReinClothSetupFlags::Failed;
		}

		TSet<FPolygonGroupID> PolygonGroups;
		for (FPolygonGroupID PolygonGroupID : MeshDescription->PolygonGroups().GetElementIDs())
		{
			if (PolygonGroupMaterialSlotNames[PolygonGroupID] == ImportedMaterialSlotName)
			{
				PolygonGroups.Add(PolygonGroupID);
			}
		}

		TSet<FVertexID> VertexIDSet;
		for (FPolygonGroupID PolygonGroupID : PolygonGroups)
		{
			for (FPolygonID PolygonID : MeshDescription->GetPolygonGroupPolygonIDs(PolygonGroupID))
			{
				for (FVertexID VertexID : MeshDescription->GetPolygonVertices(PolygonID))
				{
					VertexIDSet.Add(VertexID);
				}
			}
		}

		auto VertexIds = VertexIDSet.Array();
		auto NumVertices = VertexIds.Num();
		if (NumVertices <= 0)
		{
			OutMessage = FString::Format(TEXT("Section {0}: 不正な頂点数"), { Section.UniqueName.ToString() });
			return EReinClothSetupFlags::Failed;
		}

		Section.GridSize = FMath::CeilToInt(FMath::Sqrt(static_cast<float>(NumVertices)));
		Section.GridSize = FMath::RoundUpToPowerOfTwo(Section.GridSize);

		for (FPolygonGroupID PolygonGroupID : PolygonGroups)
		{
			for (FPolygonID PolygonID : MeshDescription->GetPolygonGroupPolygonIDs(PolygonGroupID))
			{
				for (FVertexInstanceID VertexInstanceID : MeshDescription->GetPolygonVertexInstances(PolygonID))
				{
					auto VertexID = MeshDescription->GetVertexInstanceVertex(VertexInstanceID);
					auto FlattenUV = VertexIds.Find(VertexID);
					if (FlattenUV == INDEX_NONE)
					{
						continue;
					}

					auto X = FlattenUV % Section.GridSize;
					auto Y = FlattenUV / Section.GridSize;

					auto U = static_cast<float>(X) / static_cast<float>(Section.GridSize);
					auto V = static_cast<float>(Y) / static_cast<float>(Section.GridSize);

					VertexInstanceUVs.Set(VertexInstanceID, ClothUVChannel, FVector2f(U, V));
				}
			}
		}
	}

	USkeletalMesh::FCommitMeshDescriptionParams Params;
	Params.bUpdateMorphTargets = false;
	Params.bUpdateVertexColors = false;
	Params.bForceUpdate = true;

	SkeletalRenderMeshAsset->CommitMeshDescription(LODIndex, Params);
	SkeletalRenderMeshAsset->PostEditChange();
	SkeletalRenderMeshAsset->MarkPackageDirty();

	Guid = NewGuid;

	return EReinClothSetupFlags::Newly;
#else  // WITH_EDITOR
	return EReinClothSetupFlags::Already;
#endif  // WITH_EDITOR
}

void UReinClothDataAsset::Setup()
{
	Setup_Impl(false);
}

void UReinClothDataAsset::SetupForce()
{
	Setup_Impl(true);
}

void UReinClothDataAsset::Setup_Impl(bool bIsForce)
{
#if WITH_EDITOR
	Message = TEXT("");

	if (!IsValid(SkeletalRenderMeshAsset))
	{
		// 描画メッシュが存在しない場合は不可
		const auto Property = GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UReinClothDataAsset, SkeletalRenderMeshAsset));
		auto DisplayName = Property ? Property->GetDisplayNameText().ToString() : TEXT("Unknown");
		Message = FString::Format(TEXT("{0}\nがセットされていないため失敗しました。"), { DisplayName });
		return;
	}

	if (!IsValid(SkeletalSimulationMeshAsset))
	{
		// シミュレーションメッシュが存在しない場合は不可
		const auto Property = GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UReinClothDataAsset, SkeletalSimulationMeshAsset));
		auto DisplayName = Property ? Property->GetDisplayNameText().ToString() : TEXT("Unknown");
		Message = FString::Format(TEXT("{0}\nがセットされていないため失敗しました。"), { DisplayName });
		return;
	}

#pragma region スロット名からスロットインデックスを検索
	for (auto& Section : Sections)
	{
		Section.RenderSection = ReinCloth::FindSectionIndexBySlotName(SkeletalRenderMeshAsset, Section.RenderSlotName);
		if (Section.RenderSection == INDEX_NONE)
		{
			auto Format = TEXT(
				"Unique Name: {0}\n"
				"Skeletal Render Mesh: {1}\n"
				"Render Slot Name: {2}\n"
				"SkeletalRenderMesh にスロット名 {2} が見つかりません。");
			Message = FString::Format(Format,
				{
					Section.UniqueName.ToString(),
					SkeletalRenderMeshAsset->GetName(),
					Section.RenderSlotName.ToString(),
				});
			return;
		}

		for (auto& Piece : Section.Pieces)
		{
			Piece.SimulationSection = ReinCloth::FindSectionIndexBySlotName(SkeletalSimulationMeshAsset, Piece.SimulationSlotName);
			if (Piece.SimulationSection == INDEX_NONE)
			{
				auto Format = TEXT(
					"Unique Name: {0}\n"
					"Skeletal Simulation Mesh: {1}\n"
					"Simulation Slot Name: {2}\n"
					"SkeletalSimulationMesh にスロット名 {2} が見つかりません。");
				Message = FString::Format(Format,
					{
						Section.UniqueName.ToString(),
						SkeletalSimulationMeshAsset->GetName(),
						Piece.SimulationSlotName.ToString(),
					});
				return;
			}
		}
	}
#pragma endregion

	auto SetupFlags = SetupClothUV(bIsForce, Message);
	if (EnumHasAnyFlags(SetupFlags, EReinClothSetupFlags::Failed))
	{
		return;
	}

	// レンダーメッシュに変更があったらシミュは全部再セットアップ
	bIsForce = EnumHasAnyFlags(SetupFlags, EReinClothSetupFlags::Newly) ? true : bIsForce;

	for (auto& Section : Sections)
	{
		SetupFlags |= Section.Setup(SkeletalRenderMeshAsset, SkeletalSimulationMeshAsset, ClothUVChannel, static_cast<float>(Section.GridSize), bIsForce, Message);
		if (EnumHasAnyFlags(SetupFlags, EReinClothSetupFlags::Failed))
		{
			Message = FString::Format(TEXT("Section {0}: {1}"), { Section.UniqueName.ToString(), Message });
			break;
		}
	}

	if (!EnumHasAnyFlags(SetupFlags, EReinClothSetupFlags::Failed))
	{
		Message = TEXT("Success.");
	}

	if (EnumHasAnyFlags(SetupFlags, EReinClothSetupFlags::Newly))
	{
		MarkPackageDirty();
	}
#endif  // WITH_EDITOR
}

#undef LOCTEXT_NAMESPACE
