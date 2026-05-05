// Copyright © 2026 kafues511 All Rights Reserved.

#include "ReinClothCollisionComponent.h"

#if WITH_EDITOR
#include "DrawDebugHelpers.h"
#endif  // WITH_EDITOR

UReinClothCollisionComponent::UReinClothCollisionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITOR
	// note: Tickが走るのはEditor限定だから、Tickに依存した処理を作成する場合はココの分岐を切ってね
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
#else  // WITH_EDITOR
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
#endif  // WITH_EDITOR
}

void UReinClothCollisionComponent::BeginPlay()
{
	Super::BeginPlay();

	for (const auto& CollisionTag : CollisionTags)
	{
		if (!ComponentHasTag(CollisionTag))
		{
			ComponentTags.Add(CollisionTag);
		}
	}

	for (const auto& CollisionTag : CollisionTags)
	{
		if (auto Owner = GetOwner(); IsValid(Owner) && !Owner->ActorHasTag(CollisionTag))
		{
			Owner->Tags.Add(CollisionTag);
		}
	}
}

void UReinClothCollisionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

#if WITH_EDITOR
	if (!bIsVisualize)
	{
		return;
	}

	const auto World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	if (Rotation != FQuat4f(0.0f))
	{
		// note: カプセル以外のコリジョンを分岐で選べるようにすると処理速度が顕著に落ちるから、絶対にカプセル以外を追加しないで
		DrawDebugCapsule(World, FVector(Translation), Scale.Z, Scale.X, FQuat(Rotation), VisualizeColor, false, -1.0f, 255u);
	}
#endif  // WITH_EDITOR
}

void UReinClothCollisionComponent::SetTransform(const FTransform& Transform)
{
	Translation = FVector3f(Transform.GetTranslation());
	Rotation = FQuat4f(Transform.GetRotation());
	Scale = FVector3f(Transform.GetScale3D());
}
