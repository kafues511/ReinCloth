// Copyright © 2026 kafues511 All Rights Reserved.

#include "ReinClothSimulationComponent.h"
#include "Kismet/GameplayStatics.h"

#include "ReinClothSubsystem.h"
#include "ReinClothCollisionComponent.h"

UReinClothSimulationComponent::UReinClothSimulationComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UReinClothSimulationComponent::BeginPlay()
{
	Super::BeginPlay();

	// note: BeginPlayでInstanceを書き換えるとDetailsからパラメタ調整するとRollbackする不思議挙動を引き起こす
	// InitializeComponentでも同様の挙動
	// 怠いからTickでSetup叩いています。
}

void UReinClothSimulationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	auto ReinClothSubsystem = UReinClothSubsystem::GetCurrent(GetWorld());
	if (IsValid(ReinClothSubsystem))
	{
		for (auto& Instance : Instances)
		{
			Instance.Release(ReinClothSubsystem);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void UReinClothSimulationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const auto Owner = GetOwner();
	if (!IsValid(Owner))
	{
		return;
	}

	auto ReinClothSubsystem = UReinClothSubsystem::GetCurrent(GetWorld());
	if (!IsValid(ReinClothSubsystem))
	{
		return;
	}

	for (auto& Instance : Instances)
	{
		// 初期化処理
		if (!Instance.Initialize(ReinClothSubsystem, Owner))
		{
			continue;
		}

		// 更新処理
		if (!Instance.Update(ReinClothSubsystem))
		{
			continue;
		}

		// コリジョン更新
		UpdateCollision(
			Instance,
			ReinClothSubsystem->ClothSettings[Instance.GlobalSectionIndex].NumCollisions,
			ReinClothSubsystem->ClothSettings[Instance.GlobalSectionIndex].Collisions);
	}
}

void UReinClothSimulationComponent::ResetSimulation_Implementation()
{
	for (auto& Instance : Instances)
	{
		Instance.bIsResetSimulation = true;
	}
}

void UReinClothSimulationComponent::SettleSimulation_Implementation(int32 NumIterations)
{
	for (auto& Instance : Instances)
	{
		Instance.NumSettleIterations = NumIterations;
	}
}

void UReinClothSimulationComponent::UpdateCollision(const FReinClothInstance& Instance, uint32& OutNumCollisions, TStaticArray<FReinClothCollision, 512u>& OutCollisions)
{
	auto MaxCollisions = static_cast<uint32>(OutCollisions.Num());

	TArray<AActor*> CollisionOwners;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), Instance.CollisionTag, CollisionOwners);

	OutNumCollisions = 0u;
	for (auto CollisionOwner : CollisionOwners)
	{
		if (OutNumCollisions >= MaxCollisions)
		{
			// これ以上コリジョン積めない
			break;
		}

		TArray<UReinClothCollisionComponent*> Components;
		CollisionOwner->GetComponents<UReinClothCollisionComponent>(Components);
		for (auto CollisionComponent : Components)
		{
			if (!IsValid(CollisionComponent))
			{
				continue;
			}

			if (!CollisionComponent->ComponentHasTag(Instance.CollisionTag))
			{
				continue;
			}

			if (OutNumCollisions >= MaxCollisions)
			{
				// これ以上コリジョン積めない
				break;
			}

			auto AxisZ = CollisionComponent->Rotation.GetAxisZ().GetSafeNormal(1.0e-8f, FVector3f::ZAxisVector);
			auto HalfHeight = CollisionComponent->Scale.Z * 0.5f;

			auto& Collision = OutCollisions[OutNumCollisions++];
			Collision.CapsuleStart = CollisionComponent->Translation - AxisZ * HalfHeight;
			Collision.CapsuleEnd = CollisionComponent->Translation + AxisZ * HalfHeight;
			Collision.Radius = CollisionComponent->Scale.X;
			Collision.Friction = CollisionComponent->Friction;
		}
	}
}
