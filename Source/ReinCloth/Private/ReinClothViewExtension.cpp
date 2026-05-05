// Copyright © 2026 kafues511 All Rights Reserved.

#include "ReinClothViewExtension.h"
#include "PostProcess/PostProcessMaterialInputs.h"
#include "Components/SkeletalMeshComponent.h"
#include "SkeletalRenderPublic.h"

#include "ReinClothDefines.h"
#include "ReinClothSubsystem.h"

DECLARE_GPU_STAT(ReinCloth);
DECLARE_GPU_STAT_NAMED(ReinClothVisualize, TEXT("ReinCloth.Visualize"));
DECLARE_GPU_STAT_NAMED(ReinClothSolveIntegrated, TEXT("ReinCloth.SolveIntegrated"));

DECLARE_CYCLE_STAT(TEXT("Vertex Buffer RHI Lock and Copy"), STAT_ReinCloth_VertexBufferRHI_LockAndCopy, STATGROUP_ReinCloth);

DECLARE_MEMORY_STAT(TEXT("Total Memory"), STAT_ReinCloth_TotalMemory, STATGROUP_ReinCloth);

DECLARE_DWORD_COUNTER_STAT(TEXT("Num Instances"), STAT_ReinCloth_NumInstances, STATGROUP_ReinCloth);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num Positions"), STAT_ReinCloth_NumPositions, STATGROUP_ReinCloth);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num Constraints"), STAT_ReinCloth_NumConstraints, STATGROUP_ReinCloth);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num Embeddeds"), STAT_ReinCloth_NumEmbeddeds, STATGROUP_ReinCloth);

#pragma region ReinClothSolveIntegrated
class FReinClothSolveIntegratedCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FReinClothSolveIntegratedCS);
	SHADER_USE_PARAMETER_STRUCT(FReinClothSolveIntegratedCS, FGlobalShader);

	class FResetSimulation : SHADER_PERMUTATION_BOOL("RESET_SIMULATION");
	class FRequestSettle : SHADER_PERMUTATION_BOOL("REQUEST_SETTLE");
	class FEnableSoftCollision : SHADER_PERMUTATION_BOOL("ENABLE_SOFT_COLLISION");
	class FEnableHardCollision : SHADER_PERMUTATION_BOOL("ENABLE_HARD_COLLISION");
	class FEmbeddedOffsetRest : SHADER_PERMUTATION_BOOL("REIN_CLOTH_EMBEDDED_OFFSET_REST");
	using FPermutationDomain = TShaderPermutationDomain<FResetSimulation, FRequestSettle, FEnableSoftCollision, FEnableHardCollision, FEmbeddedOffsetRest>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(float, DeltaTime)
		SHADER_PARAMETER(float, InvDeltaTime)
		SHADER_PARAMETER(float, InvDeltaTimeSq)

		SHADER_PARAMETER(uint32, NumSettleIterations)
		SHADER_PARAMETER(uint32, NumSubsteps)

		SHADER_PARAMETER(uint32, NumPositions)
		SHADER_PARAMETER(uint32, NumEmbeddeds)
		SHADER_PARAMETER(uint32, MaxEmbeddedsPerPosition)
		SHADER_PARAMETER(uint32, NumCollisions)

		SHADER_PARAMETER(float, VelocityDamping)
		SHADER_PARAMETER(FVector3f, Gravity)

		SHADER_PARAMETER(float, StructuralVerticalCompliance)
		SHADER_PARAMETER(float, StructuralHorizontalCompliance)
		SHADER_PARAMETER(float, ShearCompliance)
		SHADER_PARAMETER(float, BendingVerticalCompliance)
		SHADER_PARAMETER(float, BendingHorizontalCompliance)

		SHADER_PARAMETER(float, MaxDisplacement)

		SHADER_PARAMETER(FMatrix44f, RenderMatrix)

		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float4>, PositionUAV)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float4>, VelocityUAV)

		SHADER_PARAMETER_SRV(StructuredBuffer<uint>, OffsetSRV)
		SHADER_PARAMETER_SRV(StructuredBuffer<uint>, NeighborSRV)
		SHADER_PARAMETER_SRV(StructuredBuffer<FReinClothConstraint>, ConstraintSRV)

		SHADER_PARAMETER_SRV(StructuredBuffer<float4>, OriginSRV)
		SHADER_PARAMETER_SRV(StructuredBuffer<FReinClothInfluence>, InfluenceSRV)
		SHADER_PARAMETER_SRV(StructuredBuffer<FReinClothMatrix3x4>, BoneMatrixSRV)

		SHADER_PARAMETER_SRV(StructuredBuffer<FReinClothCollision>, CollisionSRV)

		SHADER_PARAMETER_SRV(StructuredBuffer<uint>, NormalOffsetSRV)
		SHADER_PARAMETER_SRV(StructuredBuffer<uint>, NormalNeighborSRV)

		SHADER_PARAMETER_SRV(StructuredBuffer<FReinClothEmbedded>, EmbeddedSRV)
		SHADER_PARAMETER_SRV(StructuredBuffer<FReinClothInfluence>, EmbeddedInfluenceSRV)

		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, PositionTextureUAV)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, NormalTextureUAV)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		// note: コンシューマーは開発機も権限も持ってないからPCのみ
		return IsPCPlatform(Parameters.Platform) && IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM6);
	}
};

IMPLEMENT_GLOBAL_SHADER(FReinClothSolveIntegratedCS, "/Plugin/ReinCloth/Private/ReinClothSolveIntegrated.usf", "MainCS", SF_Compute);
#pragma endregion

#pragma region ReinClothVisualize
class FReinClothVisualizeVS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FReinClothVisualizeVS);
	SHADER_USE_PARAMETER_STRUCT(FReinClothVisualizeVS, FGlobalShader);

	class FVisualizeRenderMesh : SHADER_PERMUTATION_BOOL("VISUALIZE_RENDER_MESH");
	class FEmbeddedOffsetRest : SHADER_PERMUTATION_BOOL("REIN_CLOTH_EMBEDDED_OFFSET_REST");
	using FPermutationDomain = TShaderPermutationDomain<FVisualizeRenderMesh, FEmbeddedOffsetRest>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER(FMatrix44f, RenderMatrix)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, PositionSRV)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint4>, TriangleSRV)
		SHADER_PARAMETER_SRV(StructuredBuffer<FReinClothInfluence>, InfluenceSRV)
		SHADER_PARAMETER_SRV(StructuredBuffer<FReinClothMatrix3x4>, BoneMatrixSRV)
		SHADER_PARAMETER_SRV(StructuredBuffer<FReinClothEmbedded>, EmbeddedSRV)
		SHADER_PARAMETER_SRV(StructuredBuffer<FReinClothInfluence>, EmbeddedInfluenceSRV)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsPCPlatform(Parameters.Platform) && IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM6);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("REIN_CLOTH_VISUALIZE_VS"), 1);
	}
};

class FReinClothVisualizePS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FReinClothVisualizePS);
	SHADER_USE_PARAMETER_STRUCT(FReinClothVisualizePS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FVector3f, Color)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsPCPlatform(Parameters.Platform) && IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM6);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("REIN_CLOTH_VISUALIZE_PS"), 1);
	}
};

BEGIN_SHADER_PARAMETER_STRUCT(FReinClothVisualizeParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FReinClothVisualizeVS::FParameters, VSParameters)
	SHADER_PARAMETER_STRUCT_INCLUDE(FReinClothVisualizePS::FParameters, PSParameters)
END_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER(FReinClothVisualizeVS, "/Plugin/ReinCloth/Private/ReinClothVisualize.usf", "MainVS", SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FReinClothVisualizePS, "/Plugin/ReinCloth/Private/ReinClothVisualize.usf", "MainPS", SF_Pixel);
#pragma endregion

FReinClothViewExtension::FReinClothViewExtension(const FAutoRegister& AutoRegister, UReinClothSubsystem* InReinClothSubsystem)
	: FSceneViewExtensionBase(AutoRegister)
	, WeakReinClothSubsystem(InReinClothSubsystem)
{
	Sections.Empty();
}

void FReinClothViewExtension::SetupViewFamily(FSceneViewFamily& InViewFamily)
{
	check(IsInGameThread());

	// note: HitProxiesを含めるとビューポートでマウスを動かしてアクティブな操作をするとstatが不正にカウントされる
	if (InViewFamily.EngineShowFlags.HitProxies)
	{
		return;
	}

	// note: スケルトンエディタを開いた際に通過されると重複処理しちゃう
	if (InViewFamily.Scene && InViewFamily.Scene->IsEditorScene())
	{
		return;
	}

	// note: とても雑な一時停止対応
	if (InViewFamily.bWorldIsPaused)
	{
		return;
	}
	if (InViewFamily.Scene && InViewFamily.Scene->GetWorld() && InViewFamily.Scene->GetWorld()->IsPaused())
	{
		return;
	}
	if (InViewFamily.Scene && InViewFamily.Scene->GetWorld() && InViewFamily.Scene->GetWorld()->bDebugPauseExecution)
	{
		return;
	}

	auto ReinClothSubsystem = WeakReinClothSubsystem.Get();
	if (!IsValid(ReinClothSubsystem))
	{
		return;
	}

	FSnapshotCache Snapshot;

	auto NumCloths = ReinClothSubsystem->ClothSettings.Num();
	Snapshot.ClothSettings.Reserve(NumCloths);

	for (int32 Index = 0; Index < NumCloths; ++Index)
	{
		const auto& Src = ReinClothSubsystem->ClothSettings[Index];

		FReinClothSettings Dst;

		#define REIN_COPY_TO(Name) Dst.Name = Src.Name

		REIN_COPY_TO(bIsSimulation);
		REIN_COPY_TO(bIsResetSimulation);
		REIN_COPY_TO(bIsVisualizeSimulationMesh);
		REIN_COPY_TO(bIsVisualizeRenderMesh);

		REIN_COPY_TO(NumSettleIterations);
		REIN_COPY_TO(NumSubsteps);

		REIN_COPY_TO(VelocityDamping);

		REIN_COPY_TO(Gravity);

		REIN_COPY_TO(MaxDisplacement);

		REIN_COPY_TO(StructuralVerticalCompliance);
		REIN_COPY_TO(StructuralHorizontalCompliance);
		REIN_COPY_TO(ShearCompliance);
		REIN_COPY_TO(BendingVerticalCompliance);
		REIN_COPY_TO(BendingHorizontalCompliance);

		REIN_COPY_TO(CollisionMode);

		REIN_COPY_TO(NumCollisions);
		if (Src.NumCollisions > 0)
		{
			FMemory::Memcpy(Dst.Collisions.GetData(), Src.Collisions.GetData(), sizeof(FReinClothCollision) * Src.NumCollisions);
		}

		REIN_COPY_TO(PositionResource);
		REIN_COPY_TO(NormalResource);
		REIN_COPY_TO(SectionIndex);
		REIN_COPY_TO(WeakClothAsset);
		REIN_COPY_TO(WeakSkeletalMeshComponent);

		if (const auto SkeletalMeshComponent = Src.WeakSkeletalMeshComponent.Get(); IsValid(SkeletalMeshComponent))
		{
			Dst.RenderMatrix = FMatrix44f(SkeletalMeshComponent->GetRenderMatrix().GetTransposed());
		}
		else
		{
			Dst.RenderMatrix = FMatrix44f::Identity;
		}

		#undef REIN_COPY_TO

		Dst.NumSettleIterations = FMath::Max(0, Dst.NumSettleIterations);
		Dst.NumSubsteps = FMath::Max(1, Dst.NumSubsteps);

		Snapshot.ClothSettings.Add(MoveTemp(Dst));
	}

	for (const auto& [WeakSkeletalMeshComponent, BoneCache] : ReinClothSubsystem->SharedBoneCacheMap)
	{
		const auto SkeletalMeshComponent = WeakSkeletalMeshComponent.Get();
		if (!IsValid(SkeletalMeshComponent))
		{
			continue;
		}

		auto NumBoneMatrices = BoneCache.BoneMatrices.Num();
		if (NumBoneMatrices <= 0)
		{
			continue;
		}

		auto& Dst = Snapshot.BoneMatrices.Add(WeakSkeletalMeshComponent);
		Dst.SetNumUninitialized(NumBoneMatrices);
		FMemory::Memcpy(Dst.GetData(), BoneCache.BoneMatrices.GetData(), sizeof(FReinClothMatrix3x4) * NumBoneMatrices);
	}

	ENQUEUE_RENDER_COMMAND(ReinCloth_ApplyFrameSnapshot)(
		[WeakThis = SharedThis(this).ToWeakPtr(), LocalSnapshot = MoveTemp(Snapshot)](FRHICommandListImmediate&) mutable
		{
			if (TSharedPtr<FReinClothViewExtension> ViewExtension = WeakThis.Pin())
			{
				ViewExtension->ApplySnapshot_RenderThread(MoveTemp(LocalSnapshot));
			}
		});
}

void FReinClothViewExtension::PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily)
{
	check(IsInRenderingThread());

	// note: HitProxiesを含めるとビューポートでマウスを動かしてアクティブな操作をするとstatが不正にカウントされる
	if (InViewFamily.EngineShowFlags.HitProxies)
	{
		return;
	}

	// note: スケルトンエディタを開いた際に通過されると重複処理しちゃう
	if (InViewFamily.Scene && InViewFamily.Scene->IsEditorScene())
	{
		return;
	}

	// note: とても雑な一時停止対応, GTとRTのどちらで判定するべきかイマイチ分かっていない
	if (InViewFamily.bWorldIsPaused)
	{
		return;
	}
	if (InViewFamily.Scene && InViewFamily.Scene->GetWorld() && InViewFamily.Scene->GetWorld()->IsPaused())
	{
		return;
	}
	if (InViewFamily.Scene && InViewFamily.Scene->GetWorld() && InViewFamily.Scene->GetWorld()->bDebugPauseExecution)
	{
		return;
	}

	for (auto& [_, SharedBoneResource] : SharedBoneResources)
	{
		auto NumBoneMatrices = SharedBoneResource.Data.Num();
		if (NumBoneMatrices <= 0)
		{
			continue;
		}

		if (SharedBoneResource.Resource.NumData != NumBoneMatrices)
		{
			// note: 意図的にSafeRelease呼んでないだけ
			SharedBoneResource.Resource.Data.SetNumUninitialized(NumBoneMatrices);
			FMemory::Memcpy(SharedBoneResource.Resource.Data.GetData(), SharedBoneResource.Data.GetData(), sizeof(FReinClothMatrix3x4) * NumBoneMatrices);

			auto Usage = (EBufferUsageFlags::Static | EBufferUsageFlags::StructuredBuffer);
			auto Access = ERHIAccess::SRVCompute;
			auto CreateDesc = SharedBoneResource.Resource.CreateDesc(TEXT("ReinCloth.BoneMatrices"), Usage, Access);

			SharedBoneResource.Resource.Buffer = GraphBuilder.RHICmdList.CreateBuffer(CreateDesc);
			SharedBoneResource.Resource.SRV = GraphBuilder.RHICmdList.CreateShaderResourceView(SharedBoneResource.Resource.Buffer, FRHIViewDesc::CreateBufferSRV().SetTypeFromBuffer(SharedBoneResource.Resource.Buffer));
		}
		else
		{
			SCOPE_CYCLE_COUNTER(STAT_ReinCloth_VertexBufferRHI_LockAndCopy);

			uint32 Size = sizeof(FReinClothMatrix3x4) * NumBoneMatrices;
			auto BoneBuffer = (FReinClothMatrix3x4*)GraphBuilder.RHICmdList.LockBuffer(SharedBoneResource.Resource.Buffer, 0, Size, RLM_WriteOnly);
			FPlatformMemory::Memcpy(BoneBuffer, SharedBoneResource.Data.GetData(), Size);
			GraphBuilder.RHICmdList.UnlockBuffer(SharedBoneResource.Resource.Buffer);
		}
	}

	for (int32 Index = 0; Index < Sections.Num(); ++Index)
	{
		Simulation_RenderThread(GraphBuilder, InViewFamily, Index);
	}
}

void FReinClothViewExtension::ApplySnapshot_RenderThread(FSnapshotCache&& Snapshot)
{
	check(IsInRenderingThread());

#if WITH_EDITOR
	SIZE_T TotalMemoryUsage = 0;
#endif  // WITH_EDITOR

	auto NumSrcCloths = Snapshot.ClothSettings.Num();
	auto NumDstCloths = Sections.Num();
	auto NumCloths = FMath::Max(NumSrcCloths, NumDstCloths);

	for (int32 Index = 0; Index < NumCloths; ++Index)
	{
		if (!Snapshot.ClothSettings.IsValidIndex(Index))
		{
			if (Sections.IsValidIndex(Index))
			{
				// todo: 初期化処理
				Sections[Index].bIsSetup = false;
				Sections[Index].Settings.bIsSimulation = false;
			}
			continue;
		}

		if (!Sections.IsValidIndex(Index))
		{
			Sections.AddDefaulted();
		}

		auto& Src = Snapshot.ClothSettings[Index];
		auto& Dst = Sections[Index];

		Dst.Settings = MoveTemp(Src);

	#if WITH_EDITOR
		TotalMemoryUsage += Dst.Positions ? Dst.Positions->GetSize() : 0u;
		TotalMemoryUsage += Dst.Velocities ? Dst.Velocities->GetSize() : 0u;

		TotalMemoryUsage += Dst.Offsets.GetResourceSize();
		TotalMemoryUsage += Dst.Neighbors.GetResourceSize();
		TotalMemoryUsage += Dst.Constraints.GetResourceSize();

		TotalMemoryUsage += Dst.Origins.GetResourceSize();
		TotalMemoryUsage += Dst.Influences.GetResourceSize();

		TotalMemoryUsage += Dst.Collisions.GetResourceSize();

		TotalMemoryUsage += Dst.NormalOffsets.GetResourceSize();
		TotalMemoryUsage += Dst.NormalNeighbors.GetResourceSize();

		TotalMemoryUsage += Dst.Embeddeds.GetResourceSize();
		TotalMemoryUsage += Dst.EmbeddedInfluences.GetResourceSize();
	#endif  // WITH_EDITOR
	}

	for (auto It = SharedBoneResources.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.Value().SafeRelease();
			It.RemoveCurrent();
		}
	}

	for (auto& [WeakSkeletalMeshComponent, Matrices] : Snapshot.BoneMatrices)
	{
		if (!WeakSkeletalMeshComponent.IsValid())
		{
			continue;
		}

		auto& Dst = SharedBoneResources.FindOrAdd(WeakSkeletalMeshComponent);
		Dst.Data = MoveTemp(Matrices);

	#if WITH_EDITOR
		TotalMemoryUsage += Dst.GetResourceSize();
	#endif  // WITH_EDITOR
	}

#if WITH_EDITOR
	SET_MEMORY_STAT(STAT_ReinCloth_TotalMemory, TotalMemoryUsage);
#endif  // WITH_EDITOR
}

void FReinClothViewExtension::SubscribeToPostProcessingPass(EPostProcessingPass Pass, const FSceneView& InView, FPostProcessingPassDelegateArray& InOutPassCallbacks, bool bIsPassEnabled)
{
#if WITH_EDITOR
	if (InView.Family && InView.Family->EngineShowFlags.HitProxies)
	{
		return;
	}

	if (InView.Family && InView.Family->Scene && InView.Family->Scene->IsEditorScene())
	{
		return;
	}

	if (Pass == EPostProcessingPass::BeforeDOF)
	{
		InOutPassCallbacks.Add(FAfterPassCallbackDelegate::CreateRaw(this, &FReinClothViewExtension::PostProcessPass_RenderThread));
	}
#endif  // WITH_EDITOR
}

void FReinClothViewExtension::Simulation_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily, int32 SectionIndex)
{
	auto& RHICmdList = GraphBuilder.RHICmdList;
	auto& ClothSection = Sections[SectionIndex];
	auto& ClothSettings = ClothSection.Settings;

	if (!ClothSettings.bIsSimulation)
	{
		return;
	}

	auto ClothAsset = ClothSettings.WeakClothAsset.Get();
	if (!IsValid(ClothAsset))
	{
		return;
	}

	if (!ClothAsset->Sections.IsValidIndex(ClothSettings.SectionIndex))
	{
		return;
	}

	auto BoneResource = SharedBoneResources.Find(ClothSettings.WeakSkeletalMeshComponent);
	if (BoneResource == nullptr)
	{
		return;
	}

	if (BoneResource->Resource.NumData <= 0)
	{
		return;
	}

	const auto& ClothAssetSection = ClothAsset->Sections[ClothSettings.SectionIndex];

	auto bIsResetSimulation = ClothSettings.bIsResetSimulation;

	if (!ClothSection.bIsSetup)
	{
		auto NumPositions = ClothAssetSection.Positions.Num();
		auto PositionDesc = FRDGBufferDesc::CreateStructuredDesc(sizeof(FVector4f), NumPositions);
		auto PositionBuffer = GraphBuilder.CreateBuffer(PositionDesc, TEXT("ReinCloth.Positions"));
		GraphBuilder.QueueBufferUpload(PositionBuffer, ClothAssetSection.Positions.GetData(), sizeof(FVector4f) * NumPositions);
		ClothSection.Positions = GraphBuilder.ConvertToExternalBuffer(PositionBuffer);

		auto NumVelocities = NumPositions;
		auto VelocityDesc = FRDGBufferDesc::CreateStructuredDesc(sizeof(FVector4f), NumVelocities);
		auto VelocityBuffer = GraphBuilder.CreateBuffer(VelocityDesc, TEXT("ReinCloth.Velocities"));
		GraphBuilder.QueueBufferUpload(VelocityBuffer, ClothAssetSection.Velocities.GetData(), sizeof(FVector4f) * NumVelocities);
		ClothSection.Velocities = GraphBuilder.ConvertToExternalBuffer(VelocityBuffer);

		#define REIN_COPY_TO(Name, BytesPerElement) \
			if (ClothAssetSection.Name.Num() > 0) \
			{ \
				ClothSection.Name.Data.SetNumUninitialized(ClothAssetSection.Name.Num()); \
				FMemory::Memcpy(ClothSection.Name.Data.GetData(), ClothAssetSection.Name.GetData(), BytesPerElement * ClothAssetSection.Name.Num()); \
			} \

		#define REIN_CREATE_SRV_BUFFER(Name) \
			if (ClothSection.Name.Data.Num() > 0) \
			{ \
				auto Usage = (EBufferUsageFlags::Static | EBufferUsageFlags::StructuredBuffer); \
				auto Access = ERHIAccess::SRVCompute; \
				auto CreateDesc = ClothSection.Name.CreateDesc(TEXT("ReinCloth." #Name), Usage, Access); \
				ClothSection.Name.Buffer = RHICmdList.CreateBuffer(CreateDesc); \
				ClothSection.Name.SRV = RHICmdList.CreateShaderResourceView(ClothSection.Name.Buffer, FRHIViewDesc::CreateBufferSRV().SetTypeFromBuffer(ClothSection.Name.Buffer)); \
			}

		REIN_COPY_TO(Offsets, sizeof(uint32));
		REIN_COPY_TO(Neighbors, sizeof(uint32));
		REIN_COPY_TO(Constraints, sizeof(FReinClothConstraint));
		REIN_COPY_TO(Origins, sizeof(FVector4f));
		REIN_COPY_TO(Influences, sizeof(FReinClothInfluence));
		ClothSection.Collisions.Data.SetNumZeroed(ClothSettings.Collisions.Num());
		REIN_COPY_TO(NormalOffsets, sizeof(uint32));
		REIN_COPY_TO(NormalNeighbors, sizeof(uint32));
		REIN_COPY_TO(Embeddeds, sizeof(FReinClothEmbedded));
		REIN_COPY_TO(EmbeddedInfluences, sizeof(FReinClothInfluence));

		REIN_CREATE_SRV_BUFFER(Offsets);
		REIN_CREATE_SRV_BUFFER(Neighbors);
		REIN_CREATE_SRV_BUFFER(Constraints);
		REIN_CREATE_SRV_BUFFER(Origins);
		REIN_CREATE_SRV_BUFFER(Influences);
		REIN_CREATE_SRV_BUFFER(Collisions);
		REIN_CREATE_SRV_BUFFER(NormalOffsets);
		REIN_CREATE_SRV_BUFFER(NormalNeighbors);
		REIN_CREATE_SRV_BUFFER(Embeddeds);
		REIN_CREATE_SRV_BUFFER(EmbeddedInfluences);

		#undef REIN_COPY_TO
		#undef REIN_CREATE_SRV_BUFFER

		// 初回はリセット必須
		bIsResetSimulation = true;

		ClothSection.bIsSetup = true;
	}

	if (!ClothSection.bIsSetup)
	{
		return;
	}

	RDG_EVENT_SCOPE_STAT(GraphBuilder, ReinCloth, "ReinCloth");
	RDG_GPU_STAT_SCOPE(GraphBuilder, ReinCloth);

	auto GlobalShaderMap = GetGlobalShaderMap(InViewFamily.GetFeatureLevel());

#if 0
	auto DeltaTime = InViewFamily.Time.GetDeltaWorldTimeSeconds();
#else
	// note: 現状は 固定フレームレートを使用 を前提とした挙動
	auto DeltaTime = 1.0f / 60.0f;
#endif
	auto SubstepDeltaTime = DeltaTime / static_cast<float>(ClothSettings.NumSubsteps);
	auto InvSubstepDeltaTime = 1.0f / SubstepDeltaTime;
	auto InvSubstepDeltaTimeSq = 1.0f / (SubstepDeltaTime * SubstepDeltaTime);

	auto NumPositions = ClothAssetSection.Positions.Num();
	auto NumConstraints = ClothAssetSection.Constraints.Num();
	auto NumEmbeddeds = ClothAssetSection.Embeddeds.Num();

	auto bIsDisableCollision = EnumHasAnyFlags(ClothSettings.CollisionMode, EReinClothCollisionMode::Disable) || (ClothSettings.CollisionMode == EReinClothCollisionMode::None);
	auto bIsSoftCollision = !bIsDisableCollision && EnumHasAnyFlags(ClothSettings.CollisionMode, EReinClothCollisionMode::SoftCollision);
	auto bIsHardCollision = !bIsDisableCollision && EnumHasAnyFlags(ClothSettings.CollisionMode, EReinClothCollisionMode::HardCollision);

	auto NumSettleIterations = static_cast<uint32>(ClothSettings.NumSettleIterations);
	auto bIsRequestSettle = NumSettleIterations > 0u;

	INC_DWORD_STAT_BY(STAT_ReinCloth_NumInstances, 1);
	INC_DWORD_STAT_BY(STAT_ReinCloth_NumPositions, NumPositions);
	INC_DWORD_STAT_BY(STAT_ReinCloth_NumConstraints, NumConstraints);
	INC_DWORD_STAT_BY(STAT_ReinCloth_NumEmbeddeds, NumEmbeddeds);

	auto PositionBuffer = GraphBuilder.RegisterExternalBuffer(ClothSection.Positions);
	auto PositionUAV = GraphBuilder.CreateUAV(PositionBuffer);
	auto PositionSRV = GraphBuilder.CreateSRV(PositionBuffer);

	auto VelocityBuffer = GraphBuilder.RegisterExternalBuffer(ClothSection.Velocities);
	auto VelocityUAV = GraphBuilder.CreateUAV(VelocityBuffer);

	auto PositionTextureBuffer = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(ClothSettings.PositionResource->GetTexture2DRHI(), TEXT("ReinCloth.PositionTexture")));
	auto PositionTextureUAV = GraphBuilder.CreateUAV(PositionTextureBuffer, ERDGUnorderedAccessViewFlags::None, PF_A32B32G32R32F);

	auto NormalTextureBuffer = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(ClothSettings.NormalResource->GetTexture2DRHI(), TEXT("ReinCloth.NormalTexture")));
	auto NormalTextureUAV = GraphBuilder.CreateUAV(NormalTextureBuffer, ERDGUnorderedAccessViewFlags::None, PF_A32B32G32R32F);

#if 0
	auto PassFlags = (GSupportsEfficientAsyncCompute ? ERDGPassFlags::AsyncCompute : ERDGPassFlags::Compute);
#else
	auto PassFlags = ERDGPassFlags::Compute;
#endif

	if (!bIsDisableCollision && ClothSettings.NumCollisions > 0)
	{
		SCOPE_CYCLE_COUNTER(STAT_ReinCloth_VertexBufferRHI_LockAndCopy);

		uint32 Size = sizeof(FReinClothCollision) * FMath::Min(ClothSettings.NumCollisions, 512u);
		auto Buffer = (FReinClothCollision*)GraphBuilder.RHICmdList.LockBuffer(ClothSection.Collisions.Buffer, 0, Size, RLM_WriteOnly);
		FPlatformMemory::Memcpy(Buffer, ClothSettings.Collisions.GetData(), Size);
		GraphBuilder.RHICmdList.UnlockBuffer(ClothSection.Collisions.Buffer);
	}

	#pragma region ReinClothSolveIntegrated
	{
		RDG_EVENT_SCOPE_STAT(GraphBuilder, ReinClothSolveIntegrated, "ReinCloth.SolveIntegrated");
		RDG_GPU_STAT_SCOPE(GraphBuilder, ReinClothSolveIntegrated);

		FReinClothSolveIntegratedCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FReinClothSolveIntegratedCS::FResetSimulation>(bIsResetSimulation);
		PermutationVector.Set<FReinClothSolveIntegratedCS::FRequestSettle>(bIsRequestSettle);
		PermutationVector.Set<FReinClothSolveIntegratedCS::FEnableSoftCollision>(bIsSoftCollision);
		PermutationVector.Set<FReinClothSolveIntegratedCS::FEnableHardCollision>(bIsHardCollision);
		PermutationVector.Set<FReinClothSolveIntegratedCS::FEmbeddedOffsetRest>(ClothAssetSection.EmbeddedOffsetMode == EReinClothEmbeddedOffsetMode::Rest);

		TShaderMapRef<FReinClothSolveIntegratedCS> ComputeShader(GlobalShaderMap, PermutationVector);

		auto PassParameters = GraphBuilder.AllocParameters<FReinClothSolveIntegratedCS::FParameters>();
		PassParameters->DeltaTime = SubstepDeltaTime;
		PassParameters->InvDeltaTime = InvSubstepDeltaTime;
		PassParameters->InvDeltaTimeSq = InvSubstepDeltaTimeSq;

		PassParameters->NumSettleIterations = NumSettleIterations;
		PassParameters->NumSubsteps = ClothSettings.NumSubsteps;

		PassParameters->NumPositions = NumPositions;
		PassParameters->NumEmbeddeds = NumEmbeddeds;
		PassParameters->MaxEmbeddedsPerPosition = FMath::DivideAndRoundUp(NumEmbeddeds, NumPositions);
		PassParameters->NumCollisions = ClothSettings.NumCollisions;

		PassParameters->VelocityDamping = ClothSettings.VelocityDamping;
		PassParameters->Gravity = ClothSettings.Gravity;

		PassParameters->StructuralVerticalCompliance = ClothSettings.StructuralVerticalCompliance;
		PassParameters->StructuralHorizontalCompliance = ClothSettings.StructuralHorizontalCompliance;
		PassParameters->ShearCompliance = ClothSettings.ShearCompliance;
		PassParameters->BendingVerticalCompliance = ClothSettings.BendingVerticalCompliance;
		PassParameters->BendingHorizontalCompliance = ClothSettings.BendingHorizontalCompliance;

		PassParameters->MaxDisplacement = ClothSettings.MaxDisplacement;

		PassParameters->RenderMatrix = ClothSettings.RenderMatrix;

		PassParameters->PositionUAV = PositionUAV;
		PassParameters->VelocityUAV = VelocityUAV;

		PassParameters->OffsetSRV = ClothSection.Offsets.SRV;
		PassParameters->NeighborSRV = ClothSection.Neighbors.SRV;
		PassParameters->ConstraintSRV = ClothSection.Constraints.SRV;

		PassParameters->OriginSRV = ClothSection.Origins.SRV;
		PassParameters->InfluenceSRV = ClothSection.Influences.SRV;
		PassParameters->BoneMatrixSRV = BoneResource->GetSRV();

		PassParameters->CollisionSRV = ClothSection.Collisions.SRV;

		PassParameters->NormalOffsetSRV = ClothSection.NormalOffsets.SRV;
		PassParameters->NormalNeighborSRV = ClothSection.NormalNeighbors.SRV;

		PassParameters->EmbeddedSRV = ClothSection.Embeddeds.SRV;
		PassParameters->EmbeddedInfluenceSRV = ClothSection.EmbeddedInfluences.SRV;

		PassParameters->PositionTextureUAV = PositionTextureUAV;
		PassParameters->NormalTextureUAV = NormalTextureUAV;

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("ReinCloth.SolveIntegrated"),
			PassFlags,
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCountWrapped(NumPositions, 1024));
	}
	#pragma endregion

	ClothSection.Positions = GraphBuilder.ConvertToExternalBuffer(PositionBuffer);
	ClothSection.Velocities = GraphBuilder.ConvertToExternalBuffer(VelocityBuffer);
}

FScreenPassTexture FReinClothViewExtension::PostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& Inputs)
{
	Inputs.Validate();

	const FScreenPassTextureSlice SceneColorOutput = Inputs.GetSceneColorOutput(EBlendableLocation::BL_SceneColorAfterDOF);
	const FScreenPassTexture SceneColor = FScreenPassTexture::CopyFromSlice(GraphBuilder, SceneColorOutput);

	FScreenPassRenderTarget Output = Inputs.OverrideOutput;

	if (!Output.IsValid())
	{
		Output = FScreenPassRenderTarget(SceneColor, ERenderTargetLoadAction::ELoad);
	}

	RDG_EVENT_SCOPE_STAT(GraphBuilder, ReinClothVisualize, "ReinClothVisualize");
	RDG_GPU_STAT_SCOPE(GraphBuilder, ReinClothVisualize);

	const FScreenPassTextureViewport InputViewport(SceneColor);
	const FScreenPassTextureViewport OutputViewport(InputViewport);

	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(View.GetFeatureLevel());

	for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
	{
		auto& ClothSection = Sections[SectionIndex];
		auto& ClothSettings = ClothSection.Settings;

		if (!ClothSection.bIsSetup)
		{
			continue;
		}

		auto ClothAsset = ClothSettings.WeakClothAsset.Get();
		if (!IsValid(ClothAsset))
		{
			continue;
		}

		if (!ClothAsset->Sections.IsValidIndex(ClothSettings.SectionIndex))
		{
			continue;
		}

		auto BoneResource = SharedBoneResources.Find(ClothSettings.WeakSkeletalMeshComponent);
		if (BoneResource == nullptr)
		{
			continue;
		}

		if (BoneResource->Resource.NumData <= 0)
		{
			continue;
		}

		const auto& ClothAssetSection = ClothAsset->Sections[ClothSettings.SectionIndex];

		if (ClothSettings.bIsVisualizeSimulationMesh && ClothAssetSection.SimTriangles.Num() > 0)
		{
			auto NumTriangles = ClothAssetSection.SimTriangles.Num();
			auto TriangleDesc = FRDGBufferDesc::CreateStructuredDesc(sizeof(FUintVector4), NumTriangles);
			auto TriangleBuffer = GraphBuilder.CreateBuffer(TriangleDesc, TEXT("ReinCloth.Triangle"));
			GraphBuilder.QueueBufferUpload(TriangleBuffer, ClothAssetSection.SimTriangles.GetData(), sizeof(FUintVector4) * NumTriangles);
			auto TriangleSRV = GraphBuilder.CreateSRV(TriangleBuffer);

			auto PositionBuffer = GraphBuilder.RegisterExternalBuffer(ClothSection.Positions);
			auto PositionSRV = GraphBuilder.CreateSRV(PositionBuffer);

			FReinClothVisualizeVS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FReinClothVisualizeVS::FVisualizeRenderMesh>(false);
			PermutationVector.Set<FReinClothVisualizeVS::FEmbeddedOffsetRest>(false);

			TShaderMapRef<FReinClothVisualizeVS> VertexShader(GlobalShaderMap, PermutationVector);
			TShaderMapRef<FReinClothVisualizePS> PixelShader(GlobalShaderMap);

			auto PassParameters = GraphBuilder.AllocParameters<FReinClothVisualizeParameters>();
			PassParameters->VSParameters.View = View.ViewUniformBuffer;
			PassParameters->VSParameters.PositionSRV = PositionSRV;
			PassParameters->VSParameters.TriangleSRV = TriangleSRV;
			PassParameters->VSParameters.EmbeddedSRV = ClothSection.Embeddeds.SRV;
			PassParameters->PSParameters.Color = FVector3f(0.0f, 1.0f, 0.0f);
			PassParameters->PSParameters.RenderTargets[0] = Output.GetRenderTargetBinding();

			GraphBuilder.AddPass(
				RDG_EVENT_NAME("ReinClothVisualize.SimulationMesh"),
				PassParameters,
				ERDGPassFlags::Raster,
				[VertexShader, PixelShader, PassParameters, OutputViewport, NumTriangles](FRHICommandListImmediate& RHICmdList)
				{
					RHICmdList.SetViewport(OutputViewport.Rect.Min.X, OutputViewport.Rect.Min.Y, 0.0f, OutputViewport.Rect.Max.X, OutputViewport.Rect.Max.Y, 1.0f);

					FGraphicsPipelineStateInitializer GraphicsPSOInit;
					RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

					GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI();
					GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
					GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Wireframe, CM_None>::GetRHI();
					GraphicsPSOInit.PrimitiveType = PT_TriangleList;
					GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GEmptyVertexDeclaration.VertexDeclarationRHI;
					GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
					SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

					SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), PassParameters->VSParameters);
					SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), PassParameters->PSParameters);

					RHICmdList.DrawPrimitive(0, NumTriangles, 1);
				});
		}

		if (ClothSettings.bIsVisualizeRenderMesh && ClothAssetSection.RenderTriangles.Num() > 0)
		{
			auto NumTriangles = ClothAssetSection.RenderTriangles.Num();
			auto TriangleDesc = FRDGBufferDesc::CreateStructuredDesc(sizeof(FUintVector4), NumTriangles);
			auto TriangleBuffer = GraphBuilder.CreateBuffer(TriangleDesc, TEXT("ReinCloth.Triangle"));
			GraphBuilder.QueueBufferUpload(TriangleBuffer, ClothAssetSection.RenderTriangles.GetData(), sizeof(FUintVector4) * NumTriangles);
			auto TriangleSRV = GraphBuilder.CreateSRV(TriangleBuffer);

			auto PositionBuffer = GraphBuilder.RegisterExternalBuffer(ClothSection.Positions);
			auto PositionSRV = GraphBuilder.CreateSRV(PositionBuffer);

			FReinClothVisualizeVS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FReinClothVisualizeVS::FVisualizeRenderMesh>(true);
			PermutationVector.Set<FReinClothVisualizeVS::FEmbeddedOffsetRest>(ClothAssetSection.EmbeddedOffsetMode == EReinClothEmbeddedOffsetMode::Rest);

			TShaderMapRef<FReinClothVisualizeVS> VertexShader(GlobalShaderMap, PermutationVector);
			TShaderMapRef<FReinClothVisualizePS> PixelShader(GlobalShaderMap);

			auto PassParameters = GraphBuilder.AllocParameters<FReinClothVisualizeParameters>();
			PassParameters->VSParameters.View = View.ViewUniformBuffer;
			PassParameters->VSParameters.RenderMatrix = ClothSettings.RenderMatrix;
			PassParameters->VSParameters.PositionSRV = PositionSRV;
			PassParameters->VSParameters.TriangleSRV = TriangleSRV;
			PassParameters->VSParameters.InfluenceSRV = ClothSection.Influences.SRV;
			PassParameters->VSParameters.BoneMatrixSRV = BoneResource->GetSRV();
			PassParameters->VSParameters.EmbeddedSRV = ClothSection.Embeddeds.SRV;
			PassParameters->VSParameters.EmbeddedInfluenceSRV = ClothSection.EmbeddedInfluences.SRV;
			PassParameters->PSParameters.Color = FVector3f(1.0f, 0.0f, 0.0f);
			PassParameters->PSParameters.RenderTargets[0] = Output.GetRenderTargetBinding();

			GraphBuilder.AddPass(
				RDG_EVENT_NAME("ReinClothVisualize.RenderMesh"),
				PassParameters,
				ERDGPassFlags::Raster,
				[VertexShader, PixelShader, PassParameters, OutputViewport, NumTriangles](FRHICommandListImmediate& RHICmdList)
				{
					RHICmdList.SetViewport(OutputViewport.Rect.Min.X, OutputViewport.Rect.Min.Y, 0.0f, OutputViewport.Rect.Max.X, OutputViewport.Rect.Max.Y, 1.0f);

					FGraphicsPipelineStateInitializer GraphicsPSOInit;
					RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

					GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI();
					GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
					GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Wireframe, CM_None>::GetRHI();
					GraphicsPSOInit.PrimitiveType = PT_TriangleList;
					GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GEmptyVertexDeclaration.VertexDeclarationRHI;
					GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
					SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

					SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), PassParameters->VSParameters);
					SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), PassParameters->PSParameters);

					RHICmdList.DrawPrimitive(0, NumTriangles, 1);
				});
		}
	}

	return MoveTemp(Output);
}

void FReinClothViewExtension::Invalidate()
{
	if (IsInRenderingThread())
	{
		Invalidate_RenderThread();
	}
	else
	{
		check(IsInGameThread());

		WeakReinClothSubsystem = nullptr;

		ENQUEUE_RENDER_COMMAND(ReinCloth_Invalidate)(
			[WeakThis = SharedThis(this).ToWeakPtr()](FRHICommandListImmediate&)
			{
				if (TSharedPtr<FReinClothViewExtension> ViewExtension = WeakThis.Pin())
				{
					ViewExtension->Invalidate_RenderThread();
				}
			});
	}
}

void FReinClothViewExtension::Invalidate_RenderThread()
{
	check(IsInRenderingThread());

	for (auto& Section : Sections)
	{
		Section.Positions.SafeRelease();
		Section.Velocities.SafeRelease();

		Section.Offsets.SafeRelease();
		Section.Neighbors.SafeRelease();
		Section.Constraints.SafeRelease();

		Section.Origins.SafeRelease();
		Section.Influences.SafeRelease();

		Section.Collisions.SafeRelease();

		Section.NormalOffsets.SafeRelease();
		Section.NormalNeighbors.SafeRelease();

		Section.Embeddeds.SafeRelease();
		Section.EmbeddedInfluences.SafeRelease();

		Section.bIsSetup = false;
	}

	for (auto& [WeakSkeletalMeshComponent, BoneResource] : SharedBoneResources)
	{
		BoneResource.SafeRelease();
	}
	SharedBoneResources.Empty();
}
