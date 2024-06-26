// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * 
 */
class RUNTIMEGEOMETRYUTILS_API PointTriangleRelation
{
public:
 static bool IsPointInTriangle2D(FVector2f& TestPoint, FVector2f& PointA,  FVector2f& PointB, FVector2f& PointC);
};
