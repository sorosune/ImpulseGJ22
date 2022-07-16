// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Camera/CameraComponent.h"
#include "PlayerCamera.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class IMPULSEGJ22_API UPlayerCamera : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UPlayerCamera();

	/// The skeletalmesh for the player
	/// Todo: clean up the unreal object structure for player
	UStaticMeshComponent * PlayerMesh;

	///Boolean so camera does not auto rotate if standing still
	bool bHasMoved;

	///Reference to the owning player
	AActor* Owner;

	/// Reference to the camera attached to the player
	/// Todo: after merging player controls with camera use whatever pointer it uses instead
	UCameraComponent* Cam;

	FVector CameraForward;
protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

		
};
