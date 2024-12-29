// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/Character/VoxelGenerationCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Actors/Chunk.h"
#include "Objects/ChunkData.h"
#include "VoxelGen/Enums.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

AVoxelGenerationCharacter::AVoxelGenerationCharacter()
{
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);
		
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCameraComponent->SetupAttachment(GetCapsuleComponent());
	FirstPersonCameraComponent->SetRelativeLocation(FVector(-10.f, 0.f, 60.f));
	FirstPersonCameraComponent->bUsePawnControlRotation = true;

	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh1P"));
	Mesh1P->SetOnlyOwnerSee(true);
	Mesh1P->SetupAttachment(FirstPersonCameraComponent);
	Mesh1P->bCastDynamicShadow = false;
	Mesh1P->CastShadow = false;
	Mesh1P->SetRelativeLocation(FVector(-30.f, 0.f, -150.f));
}

void AVoxelGenerationCharacter::BeginPlay()
{
	Super::BeginPlay();
}

void AVoxelGenerationCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	const APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController) return;
	
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
	{
		Subsystem->AddMappingContext(InputMappingContext, 0);
	}
	
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AVoxelGenerationCharacter::Move);

		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AVoxelGenerationCharacter::Look);

		EnhancedInputComponent->BindAction(SpawnBlockAction, ETriggerEvent::Completed, this, &AVoxelGenerationCharacter::SpawnBlock);

		EnhancedInputComponent->BindAction(DestroyBlockAction, ETriggerEvent::Completed, this, &AVoxelGenerationCharacter::DestroyBlock);
	}
}


void AVoxelGenerationCharacter::Move(const FInputActionValue& Value)
{
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		AddMovementInput(GetActorForwardVector(), MovementVector.Y);
		AddMovementInput(GetActorRightVector(), MovementVector.X);
	}
}

void AVoxelGenerationCharacter::Look(const FInputActionValue& Value)
{
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void AVoxelGenerationCharacter::DestroyBlock()
{
	FHitResult HitResult;
	FVector TraceStart = FirstPersonCameraComponent->GetComponentLocation();
	FVector TraceEnd = TraceStart + FirstPersonCameraComponent->GetForwardVector() * InteractionRange;

	bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECollisionChannel::ECC_Visibility);
	if (!bHit) return;

	if (AChunk* Chunk = Cast<AChunk>(HitResult.GetActor()))
	{
		const FVector& HitLocation = HitResult.Location;
		const FVector& ImpactNormal = HitResult.ImpactNormal;
		
		float BlockScaledSize = FChunkData::BlockSize * FChunkData::BlockScale;
		FVector InteractedBlockPosition = HitLocation - ImpactNormal * (BlockScaledSize / 2);
		FIntVector InteractedWorldBlockPosition = FChunkData::GetWorldBlockPosition(InteractedBlockPosition);
		
		FIntVector LocalChunkBlockPosition = FChunkData::GetLocalBlockPosition(InteractedWorldBlockPosition);
		Chunk->DestroyBlock(LocalChunkBlockPosition);
	}
}

void AVoxelGenerationCharacter::SpawnBlock()
{
	FHitResult HitResult;
	FVector TraceStart = FirstPersonCameraComponent->GetComponentLocation();
	FVector TraceEnd = TraceStart + FirstPersonCameraComponent->GetForwardVector() * InteractionRange;

	bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECollisionChannel::ECC_Visibility);
	if (!bHit) return;

	if (AChunk* Chunk = Cast<AChunk>(HitResult.GetActor()))
	{
		const FVector& HitLocation = HitResult.Location;
		const FVector& ImpactNormal = HitResult.ImpactNormal;
		float BlockScaledSize = FChunkData::BlockSize * FChunkData::BlockScale;
		
		FVector InteractedBlockPosition = HitLocation - ImpactNormal * (BlockScaledSize / 2);
		FVector SpawnBlockPosition = HitLocation + ImpactNormal * (BlockScaledSize / 2);

		FIntVector InteractedWorldBlockPosition = FChunkData::GetWorldBlockPosition(InteractedBlockPosition);
		FIntVector WorldBlockPosition = FChunkData::GetWorldBlockPosition(SpawnBlockPosition);

		UE_LOG(LogTemp, Warning, TEXT("InteractedWorldBlockPosition: %s"), *InteractedWorldBlockPosition.ToString());
		UE_LOG(LogTemp, Warning, TEXT("WorldBlockPosition: %s"), *WorldBlockPosition.ToString());
		
		// Get the local position of the block in the chunk (can exceed the chunk size to allow for block spawning in adjacent chunks)
		FIntVector LocalChunkBlockPosition = FChunkData::GetLocalBlockPosition(InteractedWorldBlockPosition) + (WorldBlockPosition - InteractedWorldBlockPosition);
		Chunk->SpawnBlock(LocalChunkBlockPosition, EBlock::Dirt);
	}
}
