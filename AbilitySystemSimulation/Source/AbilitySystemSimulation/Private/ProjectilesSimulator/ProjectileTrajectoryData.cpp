// 2025 Yohoho Productions /  Sirkai


#include "ProjectilesSimulator/ProjectileTrajectoryData.h"

#include "Components/InstancedStaticMeshComponent.h"

bool FProjectileMove::operator==(const FProjectileMove& Other) const
{
	if (!Position.Equals(Other.Position,4.f))
	{
		return false;
	}
	if (!Velocity.Equals(Other.Velocity,4.f))
	{
		return false;
	}
	if (CurrentBounceCount != Other.CurrentBounceCount)
	{
		return false;
	}
	if (bExploded != Other.bExploded)
	{
		return false;
	}
	return true;
}

bool FProjectileMove::operator!=(const FProjectileMove& Other) const
{
	return !(*this == Other);
}

bool FProjectileStep::operator==(const FProjectileStep& Other) const
{
	if (ServerFrame != Other.ServerFrame)
	{
		return false;
	}
	if (!FMath::IsNearlyEqual(AgeMS,Other.AgeMS))
	{
		return false;
	}
	if (Move != Other.Move)
	{
		return false;
	}
	return true;
}

bool FProjectileStep::operator!=(const FProjectileStep& Other) const
{
	return !(*this == Other);
}

int32 FProjectileTrajectory::GetEntryByServerFrame(const int32& ServerFrame , FProjectileStep& FoundEntry)
{
	if (Trajectory.Num() == 0)
	{
		return INDEX_NONE;
	}

	if (Trajectory.Num() == 1 || ServerFrame <= Trajectory[0].ServerFrame)
	{
		FoundEntry = Trajectory[0];
		return 0;
	}

	if (ServerFrame >= Trajectory.Last().ServerFrame)
	{
		FoundEntry = Trajectory.Last();
		return Trajectory.Num() - 1;
	}
	
	auto Compare = [](const FProjectileStep& Elem, const int32& Value)
	{
		return Elem.ServerFrame < Value;
	};

	int32 Index = Algo::LowerBound(Trajectory, ServerFrame, Compare);

	if (Index < Trajectory.Num() && Trajectory[Index].ServerFrame == ServerFrame)
	{
		FoundEntry = Trajectory[Index];
		return Index; // Found it!
	}

	return INDEX_NONE; // Not found
}

FProjectileStep FProjectileTrajectory::GetEntryByAge(const float& TargetAge)
{
	const int32 NumKeys = Trajectory.Num();
	if (NumKeys == 0)
	{
		return FProjectileStep();
	}
	if (NumKeys == 1)
	{
		return Trajectory[0];
	}
	if (TargetAge >= Trajectory.Last().AgeMS)
	{
		return Trajectory.Last();
	}

	// Assumptions:
	// - Trajectory is sorted by Age in increasing order.
	// - Ages are unique.

	int32 First = 1;
	int32 Last = NumKeys - 1;
	int32 Count = Last - First;

	while (Count > 0)
	{
		int32 Step = Count / 2;
		int32 Middle = First + Step;

		if (TargetAge > Trajectory[Middle].AgeMS)
		{
			First = Middle + 1;
			Count -= Step + 1;
		}
		else
		{
			Count = Step;
		}
	}

	const FProjectileStep& EntryA = Trajectory[First - 1];
	const FProjectileStep& EntryB = Trajectory[First];

	const float EntryAAge = EntryA.AgeMS;
	const float EntryBAge = EntryB.AgeMS;
	const float Diff = EntryBAge - EntryAAge;
	const float Alpha = !FMath::IsNearlyZero(Diff) ? ((TargetAge - EntryAAge) / Diff) : 0.f;

	return FProjectileStep::Lerp(EntryA, EntryB, Alpha);
}

FProjectileStep FProjectileTrajectory::GetEntryByAgeWithIndex(const float& TargetAge, int32& FirstIndex)
{
	const int32 NumKeys = Trajectory.Num();
	if (NumKeys == 0)
	{
		return FProjectileStep();
	}
	if (NumKeys == 1)
	{
		FirstIndex = 0;
		return Trajectory[0];
	}
	if (TargetAge >= Trajectory.Last().AgeMS)
	{
		FirstIndex = FMath::Max(Trajectory.Num() - 1 , 0);
		return Trajectory.Last();
	}

	// Assumptions:
	// - Trajectory is sorted by Age in increasing order.
	// - Ages are unique.

	int32 First = 1;
	int32 Last = NumKeys - 1;
	int32 Count = Last - First;

	while (Count > 0)
	{
		int32 Step = Count / 2;
		int32 Middle = First + Step;

		if (TargetAge > Trajectory[Middle].AgeMS)
		{
			First = Middle + 1;
			Count -= Step + 1;
		}
		else
		{
			Count = Step;
		}
	}

	const FProjectileStep& EntryA = Trajectory[First - 1];
	const FProjectileStep& EntryB = Trajectory[First];

	const float EntryAAge = EntryA.AgeMS;
	const float EntryBAge = EntryB.AgeMS;
	const float Diff = EntryBAge - EntryAAge;
	const float Alpha = !FMath::IsNearlyZero(Diff) ? ((TargetAge - EntryAAge) / Diff) : 0.f;

	FirstIndex = First - 1;
	return FProjectileStep::Lerp(EntryA, EntryB, Alpha);
}

void FProjectileTrajectory::DrawFullTrajectory(const UWorld* World,const float& DebugLifeTime,const EProjectileCollisionShape& Shape,const FVector& Size, UInstancedStaticMeshComponent* InstancedMesh)
{
	for (int32 Index = 0; Index < Trajectory.Num(); ++Index)
	{
		// Explosion only if you started exploding ??? or previous state did not explode yet
		bool DrawExplosion = false;
		bool DrawBounce = false;
		const FProjectileStep& Entry = Trajectory[Index];
		if (Index > 0)
		{
			if (!Trajectory[Index - 1].Move.bExploded && Entry.Move.bExploded)
			{
				DrawExplosion = true;
			}
			if (Entry.Move.CurrentBounceCount > Trajectory[Index - 1].Move.CurrentBounceCount)
			{
				DrawBounce = true;
			}
		}
		else if (Entry.Move.bExploded)
		{
			DrawExplosion = true;
		}
		FColor Color = Entry.Move.bExploded ? FColor::Emerald : FColor::Red;
		if (DrawBounce)
		{
			Color = FColor::Magenta;
		}
		if (InstancedMesh)
		{
			FVector Scale = InstancedMesh->GetRelativeScale3D() * Size;
			// Mesh Sizes show full diameter, so usnig a sphere mesh with 100x100x100 size and setting scale in component to 0.01,0.01,0.01
			// will give a sphere with 1,1,1 size. so the radius of this sphere is actual 0.5 we want it to be 1 so we multiply by 2.
			// this is just to allow for easy math when figuring out the scale to set in the component.
			// just divide 1 by each axis of the mesh size. and you have your scale.
			if (Shape == EProjectileCollisionShape::ESphere)
			{
				Scale *= 2;
			}
			InstancedMesh->AddInstance(FTransform(FQuat::Identity,Entry.Move.Position,Scale),true);
		}
		if (!InstancedMesh || DrawExplosion || DrawBounce)
		{
			switch (Shape)
			{
			case EProjectileCollisionShape::EBox:
				{
					DrawDebugBox(World,Entry.Move.Position,Size * 1.1f,Color,false,DebugLifeTime);
				}
			case EProjectileCollisionShape::ESphere:
				{
					DrawDebugSphere(World,Entry.Move.Position,Size.X * 1.1f,4,Color,false,DebugLifeTime);
				}
			}
		}
		
	}
	
}

void FProjectileVisualTrajectory::UpdateFomSimTrajectory(const FProjectileTrajectory& SimTrajectory,
	const FProjectileStep& OverrideStep,const int32& OverrideIndex)
{
	check(SimTrajectory.Trajectory.IsValidIndex(OverrideIndex))
	
	Positions.Empty(SimTrajectory.Trajectory.Num());
	
	Positions.Add(OverrideStep.Move.Position);
	const int32 NextIndex = OverrideIndex + 1;
	if (NextIndex == SimTrajectory.Trajectory.Num() )
	{
		Positions.Add(SimTrajectory.Trajectory.Last().Move.Position);
		return;
	}
	if (SimTrajectory.Trajectory.Num() == 1)
	{
		return;
	}
	if (SimTrajectory.Trajectory.Num() > NextIndex)
	{
		for (int32 i = NextIndex; i < SimTrajectory.Trajectory.Num(); ++i)
		{
			Positions.Add(SimTrajectory.Trajectory[i].Move.Position);
		}
	}
}
