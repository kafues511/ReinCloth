// Copyright © 2026 kafues511 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SceneViewExtension.h"
#include "RHIFwd.h"
#include "RenderGraphResources.h"
#include "ReinClothDataAsset.h"

class UReinClothSubsystem;

template<typename T>
struct FReinClothBuffer
{
	FBufferRHIRef Buffer;
	FShaderResourceViewRHIRef SRV;
	TResourceArray<T> Data;
	int32 NumData = 0;

	FRHIBufferCreateDesc CreateDesc(const TCHAR* DebugName, EBufferUsageFlags Usage, ERHIAccess Access)
	{
		// note: この後にCreateBufferを呼ぶとパッケージ版だとSizeがゼロになるためキャッシュする, Debug版だとサイズ保持される罠
		NumData = Data.Num();
		const FRHIBufferCreateDesc CreateDesc =
			FRHIBufferCreateDesc::CreateStructured<T>(DebugName, Data.Num())
			.AddUsage(Usage)
			.SetInitActionResourceArray(&Data)
			.SetInitialState(Access);
		return CreateDesc;
	}

	SIZE_T GetResourceSize() const
	{
		SIZE_T ResourceSize = sizeof(*this);

		if (Buffer)
		{
			// Create the buffer rendering resource
		#if WITH_EDITOR
			ResourceSize += Data.Num() * sizeof(T);
		#else
			ResourceSize += NumData * sizeof(T);
		#endif
		}

		return ResourceSize;
	}

	void SafeRelease()
	{
		SRV.SafeRelease();
		Buffer.SafeRelease();
		Data.Empty();
		NumData = 0;
	}
};

struct FReinClothSimulationSection
{
	FReinClothSettings Settings;

	TRefCountPtr<FRDGPooledBuffer> Positions;
	TRefCountPtr<FRDGPooledBuffer> Velocities;

	FReinClothBuffer<uint32> Offsets;
	FReinClothBuffer<uint32> Neighbors;
	FReinClothBuffer<FReinClothConstraint> Constraints;

	FReinClothBuffer<FVector4f> Origins;
	FReinClothBuffer<FReinClothInfluence> Influences;

	FReinClothBuffer<FReinClothCollision> Collisions;

	FReinClothBuffer<uint32> NormalOffsets;
	FReinClothBuffer<uint32> NormalNeighbors;

	FReinClothBuffer<FReinClothEmbedded> Embeddeds;
	FReinClothBuffer<FReinClothInfluence> EmbeddedInfluences;

	bool bIsSetup = false;
};

class REINCLOTH_API FReinClothViewExtension : public FSceneViewExtensionBase
{
public:
	FReinClothViewExtension(const FAutoRegister& AutoRegister, UReinClothSubsystem* InReinClothSubsystem);
	virtual ~FReinClothViewExtension() = default;
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {}
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override {}
	virtual void PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override;
	virtual void SubscribeToPostProcessingPass(EPostProcessingPass Pass, const FSceneView& InView, FPostProcessingPassDelegateArray& InOutPassCallbacks, bool bIsPassEnabled) override;

public:
	void Invalidate();
	void Invalidate_RenderThread();
	void Simulation_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily, int32 SectionIndex);
	FScreenPassTexture PostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& Inputs);

private:
	struct FSnapshotCache
	{
		TArray<FReinClothSettings> ClothSettings;
		TMap<TWeakObjectPtr<USkeletalMeshComponent>, TArray<FReinClothMatrix3x4>> BoneMatrices;
	};

	void ApplySnapshot_RenderThread(FSnapshotCache&& Snapshot);

public:
	TWeakObjectPtr<UReinClothSubsystem> WeakReinClothSubsystem;

	TArray<FReinClothSimulationSection> Sections;

	struct FReinClothBoneResource
	{
		FReinClothBuffer<FReinClothMatrix3x4> Resource;
		TArray<FReinClothMatrix3x4> Data;

		FBufferRHIRef GetBuffer() const
		{
			return Resource.Buffer;
		}

		FShaderResourceViewRHIRef GetSRV() const
		{
			return Resource.SRV;
		}

		SIZE_T GetResourceSize() const
		{
			return Resource.GetResourceSize();
		}

		void SafeRelease()
		{
			Resource.SafeRelease();
		}
	};

	TMap<TWeakObjectPtr<USkeletalMeshComponent>, FReinClothBoneResource> SharedBoneResources;
};
