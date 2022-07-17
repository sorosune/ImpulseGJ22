// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerCamera.h"
#include "Camera/CameraComponent.h"

// Sets default values for this component's properties
UPlayerCamera::UPlayerCamera()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UPlayerCamera::BeginPlay()
{
	Super::BeginPlay();

	Owner = GetOwner();
	Cam = Cast<UCameraComponent>(Owner->GetComponentByClass(UCameraComponent::StaticClass()));
	PlayerMesh = Cast<UStaticMeshComponent>(Owner->GetComponentByClass(UStaticMeshComponent::StaticClass()));
	//MovementComponent = Cast<UPlayerMovementComponent>(Owner->GetComponentByClass(UPlayerMovementComponent::StaticClass()));
	if (!(Cam || PlayerMesh))
	{
		PrimaryComponentTick.bCanEverTick = false;
	}
	bHasMoved = false;
}


// Called every frame
void UPlayerCamera::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);


	//Camera auto rotates unless standing still and has moved the right stick
	FVector pvel = Owner->GetVelocity();
	if (pvel.GetAbs().GetMax() > 50)
	{
		bHasMoved = true;
	}

	CameraForward = Cam->GetForwardVector();
	CameraForward = { CameraForward.X, CameraForward.Y, 0 };
	CameraForward.Normalize();
	FVector pvelforward = { pvel.X, pvel.Y, 0 };
	pvelforward.Normalize();
	FVector pup= PlayerMesh->GetUpVector();
	pup.Normalize();
	FVector pvelright = FVector::CrossProduct(pvelforward, pup);
	pvelright.Normalize();
	float rightdp = FVector::DotProduct(pvelright, CameraForward);
	float forwarddp = FVector::DotProduct(pvelforward, CameraForward);
	if (bHasMoved && forwarddp < 0.99)
	{
		float yawAmount = 0.5 - pow(FMath::RadiansToDegrees(acos(forwarddp)) - 180, 2) / 64800;
		if (rightdp < -0.1)
		{
			Cast<APawn>(Owner)->AddControllerYawInput(FMath::RadiansToDegrees(acos(rightdp) * yawAmount * DeltaTime));
		}
		else if (rightdp > 0.1)
		{
			Cast<APawn>(Owner)->AddControllerYawInput(-FMath::RadiansToDegrees(acos(-rightdp) * yawAmount * DeltaTime));
		}
	}
	
}

