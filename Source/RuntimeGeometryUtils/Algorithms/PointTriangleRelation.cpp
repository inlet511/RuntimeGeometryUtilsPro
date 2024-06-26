// Fill out your copyright notice in the Description page of Project Settings.


#include "PointTriangleRelation.h"


bool PointTriangleRelation::IsPointInTriangle2D(FVector2f& TestPoint, FVector2f& PointA,  FVector2f& PointB, FVector2f& PointC)
{
	float a = ((TestPoint.X - PointB.X) * (PointC.Y - PointB.Y) - (TestPoint.Y - PointB.Y) * (PointC.X - PointB.X)) /
				((PointA.X - PointB.X) * (PointC.Y - PointB.Y) - (PointA.Y - PointB.Y) * (PointC.X - PointB.X));
	float b = ((TestPoint.X - PointC.X) * (PointA.Y - PointC.Y) - (TestPoint.Y - PointC.Y) *( PointA.X - PointC.X)) /
				((PointB.X - PointC.X) * (PointA.Y - PointC.Y) - (PointB.Y - PointC.Y)*(PointA.X - PointC.X));
	float c = 1 - a - b;

	if( a > 0 && a < 1 && b > 0 && b < 1 && c > 0 && c < 1)
		return true;
	
	return false;
}
