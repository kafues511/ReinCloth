// Copyright © 2026 kafues511 All Rights Reserved.

#include "ReinClothInstance.h"
#include "Kismet/KismetRenderingLibrary.h"

#include "ReinClothSubsystem.h"

bool FReinClothInstance::Initialize(UReinClothSubsystem* ReinClothSubsystem, const AActor* Owner)
{
	if (bIsValid)
	{
		return true;
	}

	bIsValid = false;
	bIsRequestMID = false;

	if (!IsValid(ReinClothSubsystem))
	{
		// ボーンの登録が出来ないからダメ
		return false;
	}

	if (!IsValid(Owner))
	{
		// OwnerがいないとMeshComponentを探せない
		return false;
	}

	const auto World = Owner->GetWorld();
	if (!IsValid(World))
	{
		// Worldがないと変位を格納するバッファの作成が出来ない
		return false;
	}

	if (!IsValid(ClothAsset))
	{
		// クロスのアセットは必須
		return false;
	}

	SectionIndex = ClothAsset->FindSectionIndex(UniqueName);
	if (SectionIndex == INDEX_NONE)
	{
		// UniqueNameと一致するクロスの情報が存在しない
		return false;
	}

	if (!IsValid(SkeletalMeshComponent))
	{
		// note: ActorでMeshをキャッシュするのは非推奨、Detailsからパラメタを調整するとActorの配下のComponentは全部破棄、再作成されるから、参照切れちゃう
		SkeletalMeshComponent = Owner->FindComponentByTag<USkeletalMeshComponent>(MeshTag);
	}
	if (!IsValid(SkeletalMeshComponent))
	{
		// SRTがほしいからMeshComponentは必須
		return false;
	}

	if (!IsValid(RootSkeletalMeshComponent))
	{
		USkinnedMeshComponent* LeaderPoseComponent = SkeletalMeshComponent;
		while (IsValid(LeaderPoseComponent) && LeaderPoseComponent->LeaderPoseComponent.IsValid())
		{
			// note: LeaderPoseをしている場合は大本まで辿らないとデリゲート発行してくれない
			LeaderPoseComponent = LeaderPoseComponent->LeaderPoseComponent.Get();
		}
		RootSkeletalMeshComponent = Cast<USkeletalMeshComponent>(LeaderPoseComponent);
	}
	if (!IsValid(RootSkeletalMeshComponent))
	{
		// ボーンを参照するためにMeshComponentは必須
		return false;
	}

	if (!bIsRegisteredSharedBoneCache)
	{
		ReinClothSubsystem->RegisterSharedBoneCache(RootSkeletalMeshComponent);
		bIsRegisteredSharedBoneCache = true;
	}

	GridSize = ClothAsset->Sections[SectionIndex].GridSize;
	if (GridSize <= 0)
	{
		// 変位の格納先を作成できない
		return false;
	}

	if (!IsValid(RT_Position))
	{
		auto TextureSize = FMath::RoundUpToPowerOfTwo(GridSize);
		RT_Position = UKismetRenderingLibrary::CreateRenderTarget2D(World, TextureSize, TextureSize, RTF_RGBA16f, FLinearColor(0.0f, 0.0f, 0.0f, 0.0f), false, true);
		UKismetRenderingLibrary::ClearRenderTarget2D(World, RT_Position);

		// MIDのリソース更新を要求
		bIsRequestMID = true;
	}
	if (!IsValid(RT_Normal))
	{
		auto TextureSize = FMath::RoundUpToPowerOfTwo(GridSize);
		RT_Normal = UKismetRenderingLibrary::CreateRenderTarget2D(World, TextureSize, TextureSize, RTF_RGBA16f, FLinearColor(0.0f, 0.0f, 0.0f, 0.0f), false, true);
		UKismetRenderingLibrary::ClearRenderTarget2D(World, RT_Normal);

		// MIDのリソース更新を要求
		bIsRequestMID = true;
	}
	if (!IsValid(RT_Position) || !IsValid(RT_Normal))
	{
		// 変位の格納先は必須
		return false;
	}

	bIsValid = true;

	return true;
}

bool FReinClothInstance::Release(UReinClothSubsystem* ReinClothSubsystem)
{
	if (!IsValid(ReinClothSubsystem))
	{
		return false;
	}

	if (bIsRegisteredSharedBoneCache)
	{
		ReinClothSubsystem->UnregisterSharedBoneCache(RootSkeletalMeshComponent);
		bIsRegisteredSharedBoneCache = false;
	}

	if (GlobalSectionIndex != INDEX_NONE)
	{
		ReinClothSubsystem->ReleaseGlobalSectionIndex(GlobalSectionIndex);
		GlobalSectionIndex = INDEX_NONE;
	}

	bIsValid = false;

	return true;
}

bool FReinClothInstance::Update(UReinClothSubsystem* ReinClothSubsystem)
{
	if (!IsValid(ReinClothSubsystem))
	{
		return false;
	}

	if (GlobalSectionIndex == INDEX_NONE)
	{
		// 空き状態はランタイムに変わる可能性あるからInitializeの外で
		GlobalSectionIndex = ReinClothSubsystem->AllocationGlobalSectionIndex();
	}
	if (GlobalSectionIndex == INDEX_NONE)
	{
		// おそらくシミュレーションの限界数に達している
		return false;
	}
	if (!ReinClothSubsystem->ClothSettings.IsValidIndex(GlobalSectionIndex))
	{
		// リクエストする際に追加してるからないことはないと思うけど念のため
		return false;
	}

	// サブシステムにパラメタを渡す
	auto& Dst = ReinClothSubsystem->ClothSettings[GlobalSectionIndex];

	Dst.bIsSimulation = bIsSimulation;
	Dst.bIsVisualizeSimulationMesh = bIsVisualizeSimulationMesh;
	Dst.bIsVisualizeRenderMesh = bIsVisualizeRenderMesh;

	Dst.NumSubsteps = NumSubsteps;

	Dst.VelocityDamping = VelocityDamping;
	Dst.AnimDeltaScale = AnimDeltaScale;
	Dst.Gravity = Gravity;

	Dst.MaxDisplacement = MaxDisplacement;

	Dst.StructuralVerticalCompliance = GetStructuralVerticalCompliance();
	Dst.StructuralHorizontalCompliance = GetStructuralHorizontalCompliance();
	Dst.ShearCompliance = GetShearCompliance();
	Dst.BendingVerticalCompliance = GetBendingVerticalCompliance();
	Dst.BendingHorizontalCompliance = GetBendingHorizontalCompliance();

	Dst.CollisionMode = static_cast<EReinClothCollisionMode>(CollisionMode);

	Dst.SectionIndex = SectionIndex;
	Dst.WeakClothAsset = ClothAsset;

	Dst.PositionResource = RT_Position->GetResource();
	Dst.NormalResource = RT_Normal->GetResource();

	Dst.WeakSkeletalMeshComponent = RootSkeletalMeshComponent;

	if (bIsRequestMID && ClothAsset->Sections.IsValidIndex(SectionIndex))
	{
		auto MID = SkeletalMeshComponent->CreateAndSetMaterialInstanceDynamic(ClothAsset->Sections[SectionIndex].RenderSection);
		if (IsValid(MID))
		{
			MID->SetTextureParameterValue(TEXT("ReinClothPosition"), RT_Position);
			MID->SetTextureParameterValue(TEXT("ReinClothNormal"), RT_Normal);
			MID->SetScalarParameterValue(TEXT("ReinClothGridSize"), static_cast<float>(GridSize));

			// リクエストの消化完了
			bIsRequestMID = false;
		}
	}

	Dst.bIsResetSimulation = bIsResetSimulation;
	bIsResetSimulation = false;  // 要求の消化完了

	Dst.NumSettleIterations = NumSettleIterations;
	NumSettleIterations = 0;  // 要求の消化完了

	return true;
}
