// Fill out your copyright notice in the Description page of Project Settings.


#include "DataTypes/TargetingTypes/TargetingDataTypes.h"

#include "NetworkPredictionReplicationProxy.h"

bool FSyncedScreenProjection::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	// data is packet into 96 bits
		
	// FOV - range 0-3276.7
	int16 FOVasInt = (int16)FMath::RoundToInt(FMath::Abs(FOV * 10)); // 0.1 precision
	// use the sign bit to send whether we want to maintain XFOV or not,
	// Assumptions : 0-3276.7 is plenty enough range for FOV , and it can never be negative or 0
	// so use the sign to keep data in exact 12 bytes.
	if (Ar.IsSaving() && bMaintainXFOV)
	{
		FOVasInt *= -1;
	}
	Ar << FOVasInt;
	FOV = FMath::Abs(FOVasInt) * 0.1f;
	bMaintainXFOV = FOVasInt < 0;
	// Aspect ratio - range 0-655.35
	uint16 PackedAR = (uint16)FMath::RoundToInt(AspectRatio * 10); // 0.1 precision
	Ar << PackedAR;
	AspectRatio = PackedAR * 0.1f;
	// View Size , 1 pixel precision - range 0-65535
	uint16 ViewSizeX = (uint16)FMath::RoundToInt(ViewSize.X); // 1 pixel precision
	uint16 ViewSizeY = (uint16)FMath::RoundToInt(ViewSize.Y); // 1 pixel precision
	Ar << ViewSizeX;
	Ar << ViewSizeY;
	ViewSize.X = ViewSizeX;
	ViewSize.Y = ViewSizeY;
	// View Min , 1 pixel precision - range 0-65535
	uint16 ViewMinX = (uint16)FMath::RoundToInt(ViewMin.X); // 1 pixel precision
	uint16 ViewMinY = (uint16)FMath::RoundToInt(ViewMin.Y); // 1 pixel precision
	Ar << ViewMinX;
	Ar << ViewMinY;
	ViewMin.X = ViewMinX;
	ViewMin.Y = ViewMinY;
		
	bOutSuccess = true;
	return true;
}

bool FSyncedScreenProjection::operator==(const FSyncedScreenProjection& Other) const
{
	return FMath::IsNearlyEqual(FOV,Other.FOV)
		&& FMath::IsNearlyEqual(AspectRatio,Other.AspectRatio)
		&& ViewSize.Equals(Other.ViewSize)
		&& ViewMin.Equals(Other.ViewMin);
}

bool FSyncedScreenProjection::operator!=(const FSyncedScreenProjection& Other) const
{
	return !(*this == Other);
}

void FSyncedScreenProjection::ToString(FAnsiStringBuilderBase& Out) const
{
	Out.Appendf("Aspect Ration = %.2f , FOV = %.2f\n", AspectRatio,FOV);
	Out.Appendf("ViewSize: X=%.2f Y=%.2f\n", ViewSize.X,ViewSize.Y);
	Out.Appendf("ViewMin: X=%.2f Y=%.2f\n", ViewMin.X,ViewMin.Y);
}

FMatrix FSyncedScreenProjection::BuildScreenProjectionMatrix(const FSyncedScreenProjection& Data,
                                                             const FRotator& ControlRotation, const FVector& CameraLocation)
{
	// Unreal-style rotation swizzle matrix
	const FMatrix ViewRotationMatrix =
		FInverseRotationMatrix(ControlRotation) *
		FMatrix(
			FPlane(0, 0, 1, 0),
			FPlane(1, 0, 0, 0),
			FPlane(0, 1, 0, 0),
			FPlane(0, 0, 0, 1)
		);

	const bool bMaintainXFOV = Data.bMaintainXFOV;
	float XAxisMultiplier;
	float YAxisMultiplier;
		
	if (bMaintainXFOV)
	{
		// If the viewport is wider than it is tall
		XAxisMultiplier = 1.0f;
		YAxisMultiplier =  Data.ViewSize.X / (float)Data.ViewSize.Y;
	}
	else
	{
		// If the viewport is taller than it is wide
		XAxisMultiplier = Data.ViewSize.Y / (float)Data.ViewSize.X;
		YAxisMultiplier = 1.0f;
	}

	float MatrixHalfFOV;
		
	if (!bMaintainXFOV && Data.AspectRatio != 0.f )
	{
		// The view-info FOV is horizontal. But if we have a different aspect ratio constraint, we need to
		// adjust this FOV value using the aspect ratio it was computed with, so we that we can compute the
		// complementary FOV value (with the effective aspect ratio) correctly.
		const float HalfXFOV = FMath::DegreesToRadians(FMath::Max(0.001f, Data.FOV) / 2.f);
		const float HalfYFOV = FMath::Atan(FMath::Tan(HalfXFOV) / Data.AspectRatio);
		MatrixHalfFOV = HalfYFOV;
	}
	else
	{
		// Avoid divide by zero in the projection matrix calculation by clamping FOV.
		// Note the division by 360 instead of 180 because we want the half-FOV.
		MatrixHalfFOV = FMath::Max(0.001f, Data.FOV) * (float)UE_PI / 360.0f;
	}

	const FMatrix ProjectionMatrix = FReversedZPerspectiveMatrix(
			MatrixHalfFOV,
			MatrixHalfFOV,
			XAxisMultiplier,
			YAxisMultiplier,
			GNearClippingPlane,
			GNearClippingPlane
		);

	return FTranslationMatrix(-CameraLocation) * ViewRotationMatrix * ProjectionMatrix;
}
