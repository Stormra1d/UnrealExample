#include "PlayerAIController.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "CollisionQueryParams.h"
#include "../FPSCharacter.h"
#include "DrawDebugHelpers.h"
#include <FPSProject/FPSEnemyBase.h>

APlayerAIController::APlayerAIController()
{
    PrimaryActorTick.bCanEverTick = true;
}

void APlayerAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    ControlledCharacter = Cast<AFPSCharacter>(InPawn);

    if (ControlledCharacter)
    {
        ControlledCharacter->CurrentHealth = ControlledCharacter->MaxHealth;
        ControlledCharacter->bIsDead = false;

        LastKnownNavMeshPosition = ControlledCharacter->GetActorLocation();
        LastValidPosition = LastKnownNavMeshPosition;

        ControlledCharacter->MovementModeChangedDelegate.AddDynamic(this, &APlayerAIController::OnMovementModeChanged);
        ControlledCharacter->LandedDelegate.AddDynamic(this, &APlayerAIController::OnJumpLanded);

        LastKnownNavMeshPosition = ControlledCharacter->GetActorLocation();
        LastValidPosition = LastKnownNavMeshPosition;
    }

    // Reset all state
    PathPoints.Empty();
    CurrentPathIndex = 0;
    bHasTarget = false;
    CurrentIntent = ENavigationIntent::Idle;
    CurrentManeuver.Reset();
    ManeuverTimer = 0.f;
    RecoveryTimer = 0.f;
    RecoveryAttempts = 0;
    TriedRecoveryDirections.Empty();
    StuckTimer = 0.f;
    bWasOnNavMeshLastFrame = true;
    bWaitingForLanding = false;
}

void APlayerAIController::SetTarget(const FVector& NewTarget)
{
    if (!ControlledCharacter) return;

    ENavigationIntent PreviousIntent = CurrentIntent;
    if (PreviousIntent == ENavigationIntent::ExecutingManeuver)
    {
        CompleteManeuver(false);
    }

    CurrentTarget = NewTarget;
    bHasTarget = true;
    CurrentIntent = ENavigationIntent::Following;

    UpdatePath();
    //LogState(FString::Printf(TEXT("New target set: %s"), *NewTarget.ToString()), FColor::Green);
}

void APlayerAIController::ClearTarget()
{
    bHasTarget = false;
    CurrentIntent = ENavigationIntent::Idle;
    CurrentManeuver.Reset();
    PathPoints.Empty();
    LogState(TEXT("Target cleared"), FColor::Yellow);
}

void APlayerAIController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!ControlledCharacter) return;

    FVector CurrentLocation = ControlledCharacter->GetActorLocation();
    bool bOnNavMesh = IsOnNavMesh(CurrentLocation);

    // ALWAYS try to engage enemies, regardless of other states
    bool bEngagingEnemy = TryEngageNearbyEnemy();

    if (bOnNavMesh)
    {
        LastKnownNavMeshPosition = CurrentLocation;
        LastValidPosition = CurrentLocation;
    }

    if (!bEngagingEnemy) {
        float MovementThisFrame = FVector::Dist(CurrentLocation, LastValidPosition);
        if (MovementThisFrame < 5.f && !LastMovementDirection.IsZero())
        {
            StuckTimer += DeltaTime;
        }
        else
        {
            StuckTimer = 0.f;
            LastValidPosition = CurrentLocation;
        }

        if (StuckTimer > StuckThreshold && CurrentIntent != ENavigationIntent::EmergencyRecovery)
        {
            //LogState(TEXT("STUCK! Starting emergency recovery"), FColor::Red);
            StartEmergencyRecovery();
        }
    }
    else {
        StuckTimer = 0.f;
    }

    if (!bEngagingEnemy) {
        switch (CurrentIntent)
        {
        case ENavigationIntent::Following:
            ProcessNavigation(DeltaTime);
            break;
        case ENavigationIntent::ExecutingManeuver:
            ExecuteManeuver(DeltaTime);
            break;
        case ENavigationIntent::EmergencyRecovery:
            HandleEmergencyRecovery(DeltaTime);
            break;
        case ENavigationIntent::Idle:
        default:
            break;
        }
    }

    bWasOnNavMeshLastFrame = bOnNavMesh;

    if (bDebugVisualization)
    {
        DrawDebugInfo();
    }
}

void APlayerAIController::UpdatePath()
{
    if (!ControlledCharacter || !bHasTarget) return;

    UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
    if (!NavSys) return;

    CurrentNavPath = NavSys->FindPathToLocationSynchronously(
        GetWorld(),
        ControlledCharacter->GetActorLocation(),
        CurrentTarget
    );

    PathPoints.Empty();
    if (HasValidPath())
    {
        PathPoints = CurrentNavPath->PathPoints;
        CurrentPathIndex = 1; // Skip first point (current location)
        //LogState(FString::Printf(TEXT("Path updated: %d points"), PathPoints.Num()), FColor::Blue);
    }
    else
    {
        //LogState(TEXT("No valid path found - starting emergency recovery"), FColor::Red);
        StartEmergencyRecovery();
    }
}

void APlayerAIController::ProcessNavigation(float DeltaTime)
{
    if (!HasValidPath() || CurrentPathIndex >= PathPoints.Num())
    {
        if (bHasTarget)
        {
            FVector CurrentLocation = ControlledCharacter->GetActorLocation();
            if (FVector::Dist(CurrentLocation, CurrentTarget) < 100.f)
            {
                //LogState(TEXT("Target reached!"), FColor::Green);
                ClearTarget();
            }
            else
            {
                //LogState(TEXT("Path invalid but target not reached - checking for drop opportunity"), FColor::Orange);

                FVector DirectionToTarget = (CurrentTarget - CurrentLocation).GetSafeNormal2D();

                FPathChallenge DropChallenge = CheckForDropToTarget(DirectionToTarget);
                if (DropChallenge.IsValid() && DropChallenge.RequiredAction == EManeuverType::Drop)
                {
                    FManeuverPlan Plan = PlanManeuver(DropChallenge);
                    if (Plan.IsValid() && ValidateManeuverPlan(Plan))
                    {
                        //LogState(TEXT("Found drop opportunity toward target!"), FColor::Green);
                        StartManeuver(Plan);
                        return;
                    }
                }

                UpdatePath();
            }
        }
        return;
    }

    FVector NextPoint = PathPoints[CurrentPathIndex];
    FVector CurrentLocation = ControlledCharacter->GetActorLocation();
    FVector DirectionToWaypoint = (NextPoint - CurrentLocation).GetSafeNormal2D();

    if (!DirectionToWaypoint.IsZero())
    {
        FRotator TargetRotation = DirectionToWaypoint.Rotation();
        FRotator CurrentRotation = ControlledCharacter->GetActorRotation();

        TargetRotation.Pitch = CurrentRotation.Pitch;
        TargetRotation.Roll = CurrentRotation.Roll;

        // Interpolation
        float RotationSpeed = 180.f; // degrees per second
        FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, RotationSpeed / 180.f);

        ControlledCharacter->SetActorRotation(NewRotation);

        //LogState(FString::Printf(TEXT("Rotating toward waypoint: Current=%.1f, Target=%.1f"),
         //   CurrentRotation.Yaw, TargetRotation.Yaw), FColor::Purple);
    }

    // Check if we've reached the current waypoint
    if (IsCloseToTarget(NextPoint))
    {
        CurrentPathIndex++;
        //LogState(FString::Printf(TEXT("Waypoint reached, advancing to %d"), CurrentPathIndex), FColor::Green);
        return;
    }

    // Look ahead for challenges
    if (!DirectionToWaypoint.IsZero())
    {
        FPathChallenge Challenge = AnalyzePathAhead(150.f, DirectionToWaypoint); // Pass direction
        if (Challenge.IsValid() && Challenge.RequiredAction != EManeuverType::Walk)
        {
            FManeuverPlan Plan = PlanManeuver(Challenge);
            if (Plan.IsValid() && ValidateManeuverPlan(Plan))
            {
                //LogState(FString::Printf(TEXT("Starting maneuver: %d"), (int32)Plan.Type), FColor::Cyan);
                StartManeuver(Plan);
                return;
            }
            else
            {
                //LogState(TEXT("Cannot plan safe maneuver - emergency recovery"), FColor::Red);
                StartEmergencyRecovery();
                return;
            }
        }
    }

    if (!DirectionToWaypoint.IsZero())
    {
        LastMovementDirection = DirectionToWaypoint;
        MoveInWorldDirection(DirectionToWaypoint);
    }

    // Handle crouching state
    if (ControlledCharacter->bIsCrouchedCustom)
    {
        LogState("Is Crouched", FColor::Red);

        CrouchTimer += DeltaTime;
        if (CrouchTimer > 1.f)
        {
            bool bCanStandNow = ControlledCharacter->CanStandUp();

            FString Formatted = FString::Printf(TEXT("CanStandUp() returned: %s"),
                bCanStandNow ? TEXT("TRUE") : TEXT("FALSE"));
            //LogState(Formatted, FColor::Red);

            if (bCanStandNow)
            {
                ControlledCharacter->ToggleCrouch();
                CrouchTimer = 0.f;
            }
            bWasBlockedFromStanding = !bCanStandNow;
        }
    }
    else
    {
        bWasBlockedFromStanding = false;
        CrouchTimer = 0.f;
    }
}

FPathChallenge APlayerAIController::AnalyzePathAhead(float LookaheadDistance, const FVector& MovementDirection)
{
    FPathChallenge Challenge;

    if (!ControlledCharacter) return Challenge;

    FVector CurrentLocation = ControlledCharacter->GetActorLocation();
    FVector ActualForwardVector = ControlledCharacter->GetActorForwardVector();

    FVector MovementDir = MovementDirection.IsZero() ? ActualForwardVector : MovementDirection;

    //LogState(FString::Printf(TEXT("AnalyzePathAhead: Movement=%s, Forward=%s"),
    //    *MovementDir.ToString(), *ActualForwardVector.ToString()), FColor::Black);

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(ControlledCharacter);
    const UWorld* World = GetWorld();
    float CapsuleHalfHeight = ControlledCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

    // Check for obstacles ONLY in the movement direction
    FHitResult MovementHit;
    FVector TraceStart = CurrentLocation;
    FVector TraceEnd = TraceStart + MovementDir * 150.f;

    bool bObstacleInMovementPath = false;
    if (World->LineTraceSingleByChannel(MovementHit, TraceStart, TraceEnd, ECC_Visibility, Params))
    {
        if (MovementHit.GetActor())
        {
            bObstacleInMovementPath = true;
            FVector Origin, BoxExtent;
            MovementHit.GetActor()->GetActorBounds(false, Origin, BoxExtent);
            float ObstacleHeight = (Origin.Z + BoxExtent.Z) - (CurrentLocation.Z - CapsuleHalfHeight);

            if (ObstacleHeight > 20.f && ObstacleHeight < MaxJumpHeight)
            {
                Challenge.Position = MovementHit.ImpactPoint;
                Challenge.RequiredAction = EManeuverType::Jump;
                Challenge.DistanceFromStart = FVector::Dist(CurrentLocation, MovementHit.ImpactPoint);
                Challenge.Description = FString::Printf(TEXT("Jump obstacle in movement path (%.1f height)"), ObstacleHeight);
                //LogState(FString::Printf(TEXT("Jump challenge detected in movement direction: %s"), *Challenge.Description), FColor::Yellow);
                return Challenge;
            }
        }
    }

    // Check for crouch obstacles in movement direction
    FVector HeadCheckStart = CurrentLocation + FVector(0, 0, CapsuleHalfHeight * 0.8f);
    FVector HeadCheckEnd = HeadCheckStart + MovementDir * 120.f;

    if (World->LineTraceSingleByChannel(MovementHit, HeadCheckStart, HeadCheckEnd, ECC_Visibility, Params))
    {
        if (MovementHit.GetActor())
        {
            FVector Origin, BoxExtent;
            MovementHit.GetActor()->GetActorBounds(false, Origin, BoxExtent);
            float ClearanceHeight = (Origin.Z - BoxExtent.Z) - (CurrentLocation.Z - CapsuleHalfHeight);
            float CrouchHeight = CapsuleHalfHeight;
            float StandHeight = CapsuleHalfHeight * 2.f;

            if (ClearanceHeight > CrouchHeight && ClearanceHeight < StandHeight)
            {
                Challenge.Position = MovementHit.ImpactPoint;
                Challenge.RequiredAction = EManeuverType::Crouch;
                Challenge.DistanceFromStart = FVector::Dist(CurrentLocation, MovementHit.ImpactPoint);
                Challenge.Description = FString::Printf(TEXT("Crouch obstacle in movement path (%.1f clearance)"), ClearanceHeight);
               // LogState(FString::Printf(TEXT("Crouch challenge detected in movement direction: %s"), *Challenge.Description), FColor::Yellow);
                return Challenge;
            }
        }
    }

    // This handles cases where character facing doesn't match movement
    if (!bObstacleInMovementPath && !MovementDir.Equals(ActualForwardVector, 0.1f))
    {
        //LogState(TEXT("No obstacle in movement path, checking facing direction for environmental hazards"), FColor::Black);

        HeadCheckEnd = HeadCheckStart + ActualForwardVector * 120.f;
        if (World->LineTraceSingleByChannel(MovementHit, HeadCheckStart, HeadCheckEnd, ECC_Visibility, Params))
        {
            if (MovementHit.GetActor())
            {
                FVector Origin, BoxExtent;
                MovementHit.GetActor()->GetActorBounds(false, Origin, BoxExtent);
                float ClearanceHeight = (Origin.Z - BoxExtent.Z) - (CurrentLocation.Z - CapsuleHalfHeight);
                float CrouchHeight = CapsuleHalfHeight;
                float StandHeight = CapsuleHalfHeight * 2.f;

                if (ClearanceHeight > CrouchHeight && ClearanceHeight < StandHeight)
                {
                    Challenge.Position = MovementHit.ImpactPoint;
                    Challenge.RequiredAction = EManeuverType::Crouch;
                    Challenge.DistanceFromStart = FVector::Dist(CurrentLocation, MovementHit.ImpactPoint);
                    Challenge.Description = FString::Printf(TEXT("Crouch obstacle in facing direction (%.1f clearance)"), ClearanceHeight);
                    //LogState(FString::Printf(TEXT("Crouch challenge detected in facing direction: %s"), *Challenge.Description), FColor::Yellow);
                    return Challenge;
                }
            }
        }
    }

    TArray<float> CheckDistances = { 50.f, 100.f, 150.f };
    for (float Distance : CheckDistances)
    {
        FVector TestPoint = CurrentLocation + MovementDir * Distance;
       // LogState(FString::Printf(TEXT("Testing drop at %.1f units: %s, OnNavMesh: %s"),
     //       Distance, *TestPoint.ToString(), IsOnNavMesh(TestPoint) ? TEXT("YES") : TEXT("NO")), FColor::Black);

        if (!IsOnNavMesh(TestPoint))
        {
           // LogState(TEXT("Found NavMesh edge - checking if it's a legitimate drop"), FColor::Yellow);

            FVector GroundLocation;
            if (HasGroundAtLocation(TestPoint, MaxSafeDropHeight, &GroundLocation))
            {
                float DropDistance = TestPoint.Z - GroundLocation.Z;
                if (DropDistance > MinDropHeight && DropDistance < MaxSafeDropHeight)
                {
                    if (HasNavMeshAtLocation(GroundLocation))
                    {
                        if (HasValidPath() && CurrentPathIndex < PathPoints.Num())
                        {
                            FVector NextWaypoint = PathPoints[CurrentPathIndex];
                            float HorizontalDistToWaypoint = FVector::Dist2D(CurrentLocation, NextWaypoint);

                            if (HorizontalDistToWaypoint < 100.f)
                            {
                               // LogState(TEXT("Too close to waypoint horizontally - skipping drop"), FColor::Orange);
                                continue;
                            }
                        }

                        Challenge.Position = TestPoint;
                        Challenge.RequiredAction = EManeuverType::Drop;
                        Challenge.DistanceFromStart = Distance;
                        Challenge.Description = FString::Printf(TEXT("Safe drop (%.1f distance)"), DropDistance);
                       // LogState(FString::Printf(TEXT("Drop challenge detected: %s"), *Challenge.Description), FColor::Yellow);
                        return Challenge;
                    }
                }
            }

            //LogState(TEXT("Edge found but not a safe drop - stopping drop analysis"), FColor::Orange);
            break;
        }
    }

    //LogState(TEXT("No challenges detected - walking"), FColor::Black);
    Challenge.RequiredAction = EManeuverType::Walk;
    return Challenge;
}

FManeuverPlan APlayerAIController::PlanManeuver(const FPathChallenge& Challenge)
{
    FManeuverPlan Plan;
    Plan.Type = Challenge.RequiredAction;
    Plan.StartPosition = ControlledCharacter->GetActorLocation();

    switch (Challenge.RequiredAction)
    {
    case EManeuverType::Jump:
        if (AnalyzeJumpObstacle(Challenge.Position, Plan))
        {
            Plan.ExpectedDuration = 2.f;
            Plan.bRequiresOffNavMeshMovement = true;
        }
        break;

    case EManeuverType::Drop:
        if (AnalyzeDropOpportunity(Challenge.Position, Plan))
        {
            Plan.ExpectedDuration = 1.5f;
            Plan.bRequiresOffNavMeshMovement = true;
        }
        break;

    case EManeuverType::Crouch:
        if (AnalyzeCrouchObstacle(Challenge.Position, Plan))
        {
            Plan.ExpectedDuration = 3.f;
            Plan.bRequiresOffNavMeshMovement = false;
        }
        break;

    default:
        Plan.Reset();
        break;
    }

    return Plan;
}

bool APlayerAIController::ValidateManeuverPlan(const FManeuverPlan& Plan)
{
    if (!Plan.IsValid()) return false;

    switch (Plan.Type)
    {
    case EManeuverType::Jump:
        return IsJumpSafe(Plan.StartPosition, Plan.TargetPosition);

    case EManeuverType::Drop:
        return IsDropSafe(Plan.StartPosition, Plan.TargetPosition);

    case EManeuverType::Crouch:
        return true;

    default:
        return false;
    }
}

void APlayerAIController::StartManeuver(const FManeuverPlan& Plan)
{
    CurrentManeuver = Plan;
    CurrentIntent = ENavigationIntent::ExecutingManeuver;
    ManeuverStartTime = GetWorld()->GetTimeSeconds();
    ManeuverTimer = 0.f;
    ManeuverStartPosition = ControlledCharacter->GetActorLocation();

    switch (Plan.Type)
    {
    case EManeuverType::Jump:
        ControlledCharacter->StartJump();
        bWaitingForLanding = true;
        break;

    case EManeuverType::Drop:
        bWaitingForLanding = true;
        if (UCharacterMovementComponent* MoveComp = ControlledCharacter->GetCharacterMovement())
        {
            if (MoveComp->IsMovingOnGround())
            {
                MoveComp->SetMovementMode(MOVE_Falling);
            }
        }
        break;

    case EManeuverType::Crouch:
        if (!ControlledCharacter->bIsCrouchedCustom)
        {
            ControlledCharacter->ToggleCrouch();
        }
        break;
    }

    //LogState(FString::Printf(TEXT("Maneuver started: %d"), (int32)Plan.Type), FColor::Magenta);
}

void APlayerAIController::ExecuteManeuver(float DeltaTime)
{
    if (!CurrentManeuver.IsValid())
    {
       // LogState("WE ARE HAVING AN INVALID MANEUVER HERE", FColor::Red);
        CompleteManeuver(false);
        return;
    }

    ManeuverTimer += DeltaTime;

    if (ManeuverTimer > CurrentManeuver.ExpectedDuration || ManeuverTimer > MaxManeuverDuration)
    {
        //LogState(TEXT("Maneuver timed out"), FColor::Orange);
        CompleteManeuver(false);
        return;
    }

    if (!CurrentManeuver.MovementDirection.IsZero())
    {
        MoveInWorldDirection(CurrentManeuver.MovementDirection);
    }

    FVector CurrentLocation = ControlledCharacter->GetActorLocation();

    switch (CurrentManeuver.Type)
    {
    case EManeuverType::Jump:
    case EManeuverType::Drop:
        if (!bWaitingForLanding)
        {
            if (ManeuverTimer > 0.5f)
            {
                FVector CurrentLocation2 = ControlledCharacter->GetActorLocation();
                bool bOnNavMesh = IsOnNavMesh(CurrentLocation2);

                LogState(FString::Printf(TEXT("Maneuver completion check - OnNavMesh: %s, Timer: %.2f"),
                    bOnNavMesh ? TEXT("YES") : TEXT("NO"), ManeuverTimer), FColor::Purple);

                CompleteManeuver(bOnNavMesh);
            }
        }
        else
        {
            if (ManeuverTimer > 3.0f) // If falling for more than 3 seconds, something's wrong
            {
               // LogState(TEXT("Maneuver taking too long - forcing completion"), FColor::Red);
                bWaitingForLanding = false;
                CompleteManeuver(false);
            }
        }
        break;

    case EManeuverType::Crouch:
        if (!CurrentManeuver.MovementDirection.IsZero())
        {
            MoveInWorldDirection(CurrentManeuver.MovementDirection);
        }

        if (ManeuverTimer > 0.5f)
        {
            bool bCanStand = ControlledCharacter->CanStandUp();

            if (bCanStand)
            {
                bool bOnNavMesh = IsOnNavMesh(ControlledCharacter->GetActorLocation());

                if (bOnNavMesh)
                {
                    LogState(TEXT("Crouch clear and on NavMesh. Completing maneuver."), FColor::Green);

                    if (ControlledCharacter->bIsCrouchedCustom)
                    {
                        ControlledCharacter->ToggleCrouch();
                    }
                    CompleteManeuver(true);
                }
                else
                {
                    LogState(TEXT("Can stand, but not on NavMesh. Continuing forward."), FColor::Orange);
                }
            }
        }
        break;
    }
}

void APlayerAIController::CompleteManeuver(bool bSuccess)
{
    EManeuverType CompletedType = CurrentManeuver.Type;
    CurrentManeuver.Reset();
    ManeuverTimer = 0.f;
    bWaitingForLanding = false;

    if (bSuccess)
    {
        //LogState(FString::Printf(TEXT("Maneuver completed successfully: %d"), (int32)CompletedType), FColor::Green);
        CurrentIntent = ENavigationIntent::Following;

        UpdatePath();
    }
    else
    {
      // LogState(FString::Printf(TEXT("Maneuver failed: %d"), (int32)CompletedType), FColor::Red);
        StartEmergencyRecovery();
    }
}

bool APlayerAIController::AnalyzeJumpObstacle(const FVector& ObstacleLocation, FManeuverPlan& OutPlan)
{
    FVector CurrentLocation = ControlledCharacter->GetActorLocation();
    FVector ForwardVector = (ObstacleLocation - CurrentLocation).GetSafeNormal2D();

    // Predict landing location
    FVector LandingCheckStart = CurrentLocation + ForwardVector * 200.f;
    LandingCheckStart.Z = CurrentLocation.Z + 50.f;

    FVector GroundLocation;
    if (HasGroundAtLocation(LandingCheckStart, MaxSafeDropHeight, &GroundLocation))
    {
        if (HasNavMeshAtLocation(GroundLocation))
        {
            OutPlan.TargetPosition = GroundLocation;
            OutPlan.MovementDirection = ForwardVector;
            OutPlan.bTargetIsOnNavMesh = true;
            return true;
        }
    }

    return false;
}

bool APlayerAIController::AnalyzeDropOpportunity(const FVector& EdgeLocation, FManeuverPlan& OutPlan)
{
    FVector GroundLocation;
    if (HasGroundAtLocation(EdgeLocation, MaxSafeDropHeight, &GroundLocation))
    {
        if (HasNavMeshAtLocation(GroundLocation))
        {
            OutPlan.TargetPosition = GroundLocation;
            OutPlan.MovementDirection = (EdgeLocation - ControlledCharacter->GetActorLocation()).GetSafeNormal2D();
            OutPlan.bTargetIsOnNavMesh = true;
            return true;
        }
    }

    return false;
}

bool APlayerAIController::AnalyzeCrouchObstacle(const FVector& ObstacleLocation, FManeuverPlan& OutPlan)
{
    FVector Direction = (ObstacleLocation - ControlledCharacter->GetActorLocation()).GetSafeNormal2D();
    OutPlan.TargetPosition = ObstacleLocation + Direction * 100.f;
    OutPlan.MovementDirection = Direction;
    OutPlan.bTargetIsOnNavMesh = true;
    return true;
}

bool APlayerAIController::IsJumpSafe(const FVector& StartPos, const FVector& LandingPos)
{
    float JumpDistance = FVector::Dist2D(StartPos, LandingPos);
    float JumpHeight = FMath::Abs(LandingPos.Z - StartPos.Z);

    return JumpDistance < 300.f && JumpHeight < MaxJumpHeight && HasNavMeshAtLocation(LandingPos);
}

bool APlayerAIController::IsDropSafe(const FVector& EdgePos, const FVector& LandingPos)
{
    float DropDistance = EdgePos.Z - LandingPos.Z;

    return DropDistance > MinDropHeight &&
        DropDistance < MaxSafeDropHeight &&
        HasNavMeshAtLocation(LandingPos);
}

bool APlayerAIController::HasNavMeshAtLocation(const FVector& Location, float Tolerance) const
{
    UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
    if (!NavSys) return false;

    FNavLocation OutLocation;
    return NavSys->ProjectPointToNavigation(Location, OutLocation, FVector(Tolerance, Tolerance, 100.f));
}

bool APlayerAIController::HasGroundAtLocation(const FVector& Location, float MaxDropDistance, FVector* OutGroundLocation) const
{
    const UWorld* World = GetWorld();
    if (!World) return false;

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(ControlledCharacter);

    FHitResult Hit;
    FVector TraceStart = Location;
    FVector TraceEnd = Location - FVector(0, 0, MaxDropDistance);

    if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params))
    {
        if (OutGroundLocation)
        {
            *OutGroundLocation = Hit.ImpactPoint;
        }
        return true;
    }

    return false;
}

void APlayerAIController::StartEmergencyRecovery()
{
    CurrentIntent = ENavigationIntent::EmergencyRecovery;
    RecoveryTimer = 0.f;
    RecoveryAttempts = 0;
    TriedRecoveryDirections.Empty();
    StuckTimer = 0.f;

    if (CurrentManeuver.IsValid())
    {
        CompleteManeuver(false);
    }

   // LogState(TEXT("EMERGENCY RECOVERY STARTED"), FColor::Red);
}

void APlayerAIController::HandleEmergencyRecovery(float DeltaTime)
{
    RecoveryTimer += DeltaTime;

    FVector CurrentLocation = ControlledCharacter->GetActorLocation();
    if (IsOnNavMesh(CurrentLocation))
    {
       // LogState(TEXT("Recovery successful - back on NavMesh"), FColor::Green);
        CurrentIntent = ENavigationIntent::Following;
        RecoveryTimer = 0.f;
        RecoveryAttempts = 0;
        TriedRecoveryDirections.Empty();
        UpdatePath();
        return;
    }

    if (RecoveryTimer > EmergencyRecoveryTimeout)
    {
     //   LogState(TEXT("Recovery timeout - teleporting to last known position"), FColor::Purple);
        if (!LastKnownNavMeshPosition.IsZero())
        {
            ControlledCharacter->SetActorLocation(LastKnownNavMeshPosition);
        }
        CurrentIntent = ENavigationIntent::Following;
        UpdatePath();
        return;
    }

    // Try to find a path back to NavMesh
    FVector RecoveryDirection;
    if (FindPathToNavMesh(RecoveryDirection))
    {
        MoveInWorldDirection(RecoveryDirection);
    //    LogState(FString::Printf(TEXT("Recovery moving: %s"), *RecoveryDirection.ToString()), FColor::Orange);
    }
    else
    {
        // Try random movement
        RecoveryAttempts++;
        if (RecoveryAttempts > 30)
        {
       //     LogState(TEXT("Too many recovery attempts - giving up"), FColor::Red);
            ClearTarget();
            return;
        }

        FVector RandomDirection = FVector(
            FMath::RandRange(-1.f, 1.f),
            FMath::RandRange(-1.f, 1.f),
            0.f
        ).GetSafeNormal2D();

        MoveInWorldDirection(RandomDirection);
        TriedRecoveryDirections.Add(RandomDirection);
    }
}

bool APlayerAIController::FindPathToNavMesh(FVector& OutDirection)
{
    FVector CurrentLocation = ControlledCharacter->GetActorLocation();
    UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
    if (!NavSys) return false;

    TArray<FVector> TestDirections;

    // Direction toward target
    if (bHasTarget)
    {
        FVector ToTarget = (CurrentTarget - CurrentLocation).GetSafeNormal2D();
        if (!ToTarget.IsZero()) TestDirections.Add(ToTarget);
    }

    // Direction toward last known navmesh position
    if (!LastKnownNavMeshPosition.IsZero())
    {
        FVector ToLastKnown = (LastKnownNavMeshPosition - CurrentLocation).GetSafeNormal2D();
        if (!ToLastKnown.IsZero()) TestDirections.Add(ToLastKnown);
    }

    // Cardinal and diagonal directions
    TArray<FVector> CardinalDirections = {
        FVector(1, 0, 0), FVector(-1, 0, 0), FVector(0, 1, 0), FVector(0, -1, 0),
        FVector(0.707f, 0.707f, 0), FVector(-0.707f, 0.707f, 0),
        FVector(0.707f, -0.707f, 0), FVector(-0.707f, -0.707f, 0)
    };

    TestDirections.Append(CardinalDirections);

    // Test each direction
    for (const FVector& Direction : TestDirections)
    {
        bool bAlreadyTried = false;
        for (const FVector& Tried : TriedRecoveryDirections)
        {
            if (FVector::DotProduct(Direction, Tried) > 0.8f)
            {
                bAlreadyTried = true;
                break;
            }
        }
        if (bAlreadyTried) continue;

        // Test multiple distances
        TArray<float> TestDistances = { 100.f, 200.f, 300.f, 500.f };
        for (float Distance : TestDistances)
        {
            FVector TestPoint = CurrentLocation + Direction * Distance;
            FNavLocation ProjectedLocation;

            if (NavSys->ProjectPointToNavigation(TestPoint, ProjectedLocation, FVector(150.f, 150.f, 100.f)))
            {
                if (FVector::Dist2D(CurrentLocation, ProjectedLocation.Location) > 50.f)
                {
                    OutDirection = Direction;
                    return true;
                }
            }
        }
    }

    return false;
}

void APlayerAIController::MoveInWorldDirection(const FVector& Direction, float Speed)
{
    if (!ControlledCharacter || Direction.IsZero()) return;

    FVector SafeDirection = Direction.GetSafeNormal2D();

    // Convert world direction to input values
    FRotator ActorRotation = ControlledCharacter->GetActorRotation();
    FVector ForwardVector = ActorRotation.Vector();
    FVector RightVector = FRotationMatrix(ActorRotation).GetUnitAxis(EAxis::Y);

    float ForwardInput = FVector::DotProduct(SafeDirection, ForwardVector) * Speed;
    float RightInput = FVector::DotProduct(SafeDirection, RightVector) * Speed;

    ControlledCharacter->MoveForward(FInputActionValue(ForwardInput));
    ControlledCharacter->MoveRight(FInputActionValue(RightInput));

    LastMovementDirection = SafeDirection;
}

bool APlayerAIController::IsOnNavMesh(const FVector& Location) const
{
    UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
    if (!NavSys) return false;

    FNavLocation OutLocation;
    return NavSys->ProjectPointToNavigation(Location, OutLocation, FVector(50.f, 50.f, 176.f));
}

FVector APlayerAIController::GetNextPathPoint() const
{
    if (HasValidPath() && CurrentPathIndex < PathPoints.Num())
    {
        return PathPoints[CurrentPathIndex];
    }
    return FVector::ZeroVector;
}

bool APlayerAIController::HasValidPath() const
{
    return CurrentNavPath && CurrentNavPath->PathPoints.Num() > 1;
}

bool APlayerAIController::IsCloseToTarget(const FVector& Target, float Tolerance) const
{
    if (!ControlledCharacter) return false;
    return FVector::DistSquared2D(Target, ControlledCharacter->GetActorLocation()) <= FMath::Square(Tolerance);
}

void APlayerAIController::OnJumpLanded(const FHitResult& Hit)
{
    if (!ControlledCharacter) return;

    bWaitingForLanding = false;
    FVector LandingLocation = ControlledCharacter->GetActorLocation();
    bool bLandedOnNavMesh = IsOnNavMesh(LandingLocation);

    LogState(FString::Printf(TEXT("Landed at %s. OnNavMesh: %s, ManeuverTimer: %.2f"),
        *LandingLocation.ToString(),
        bLandedOnNavMesh ? TEXT("YES") : TEXT("NO"),
        ManeuverTimer), FColor::Cyan);

    // Reset the maneuver timer when we land
    if (CurrentIntent == ENavigationIntent::ExecutingManeuver)
    {
        ManeuverTimer = 0.f;
     //   LogState(TEXT("Maneuver timer reset on landing"), FColor::Green);
    }

    if (CurrentIntent == ENavigationIntent::EmergencyRecovery && bLandedOnNavMesh)
    {
        CurrentIntent = ENavigationIntent::Following;
        UpdatePath();
    }
}

void APlayerAIController::OnMovementModeChanged(ACharacter* InCharacter, EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
    if (InCharacter != ControlledCharacter) return;

    UCharacterMovementComponent* MoveComp = InCharacter->GetCharacterMovement();
    if (!MoveComp) return;

    EMovementMode NewMode = MoveComp->MovementMode;

 //   LogState(FString::Printf(TEXT("Movement mode changed: %d -> %d"), (int32)PrevMovementMode, (int32)NewMode), FColor::White);

    if (NewMode == MOVE_Falling && PrevMovementMode == MOVE_Walking)
    {
        if (CurrentIntent == ENavigationIntent::Following)
        {
            StuckTimer = 0.f;
        }
    }
}

void APlayerAIController::DrawDebugInfo()
{
    if (!ControlledCharacter) return;

    const UWorld* World = GetWorld();
    if (!World) return;

    FVector CurrentLocation = ControlledCharacter->GetActorLocation();

    if (HasValidPath())
    {
        for (int32 i = 0; i < PathPoints.Num(); i++)
        {
            FColor PointColor = (i == CurrentPathIndex) ? FColor::Yellow : FColor::Green;
            DrawDebugSphere(World, PathPoints[i], 25.f, 8, PointColor, false, -1.f, 0, 2.f);

            if (i > 0)
            {
                DrawDebugLine(World, PathPoints[i - 1], PathPoints[i], FColor::Blue, false, -1.f, 0, 2.f);
            }
        }
    }

    if (bHasTarget)
    {
        DrawDebugSphere(World, CurrentTarget, 50.f, 8, FColor::Red, false, -1.f, 0, 3.f);
        DrawDebugLine(World, CurrentLocation, CurrentTarget, FColor::Orange, false, -1.f, 0, 1.f);
    }

    if (CurrentManeuver.IsValid())
    {
        DrawDebugSphere(World, CurrentManeuver.TargetPosition, 30.f, 8, FColor::Magenta, false, -1.f, 0, 2.f);
        DrawDebugLine(World, CurrentLocation, CurrentManeuver.TargetPosition, FColor::Magenta, false, -1.f, 0, 2.f);
    }

    if (!LastKnownNavMeshPosition.IsZero())
    {
        DrawDebugSphere(World, LastKnownNavMeshPosition, 20.f, 8, FColor::Cyan, false, -1.f, 0, 1.f);
    }

    FString StatusText = FString::Printf(TEXT("Intent: %d | Maneuver: %d | OnNavMesh: %s | Stuck: %.1f"),
        (int32)CurrentIntent,
        (int32)CurrentManeuver.Type,
        IsOnNavMesh(CurrentLocation) ? TEXT("YES") : TEXT("NO"),
        StuckTimer);

    DrawDebugString(World, CurrentLocation + FVector(0, 0, 100), StatusText, nullptr, FColor::White, -1.f);
}

void APlayerAIController::LogState(const FString& Message, FColor Color)
{
    if (GEngine)
    {
        FString FullMessage = FString::Printf(TEXT("[AI] %s"), *Message);
        GEngine->AddOnScreenDebugMessage(-1, 2.f, Color, FullMessage);
    }
}

FVector APlayerAIController::FindActualEdgePosition(const FVector& StartPos, const FVector& EndPos)
{
    const UWorld* World = GetWorld();
    if (!World)
    {
        LogState(TEXT("World is null in FindActualEdgePosition"), FColor::Red);
        return EndPos;
    }

//    LogState(FString::Printf(TEXT("FindActualEdgePosition called. Start: %s, End: %s"),
   //     *StartPos.ToString(), *EndPos.ToString()), FColor::Cyan);

    FVector Direction = (EndPos - StartPos).GetSafeNormal2D();
    float TotalDistance = FVector::Dist2D(StartPos, EndPos);

    float StepSize = 10.f;
    for (float CurrentDist = 0.f; CurrentDist < TotalDistance; CurrentDist += StepSize)
    {
        FVector TestPos = StartPos + Direction * CurrentDist;

        if (!IsOnNavMesh(TestPos))
        {
            LogState(FString::Printf(TEXT("NavMesh edge found at dist: %.1f, pos: %s"),
                CurrentDist, *TestPos.ToString()), FColor::Yellow);

            FCollisionQueryParams Params;
            Params.AddIgnoredActor(ControlledCharacter);

            FHitResult Hit;
            FVector TraceStart = TestPos;
            FVector TraceEnd = TestPos - FVector(0, 0, 50.f);

            if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params))
            {
                LogState(FString::Printf(TEXT("Edge hit found. Impact point: %s"), *Hit.ImpactPoint.ToString()), FColor::Green);
                return Hit.ImpactPoint;
            }
            else
            {
                LogState(TEXT("No ground hit at edge, using test position instead"), FColor::Orange);
                return TestPos;
            }
        }
        else
        {
            DrawDebugSphere(World, TestPos, 5.f, 8, FColor::Blue, false, 1.f);
        }
    }

    LogState(TEXT("No navmesh edge found, returning EndPos"), FColor::Red);
    return EndPos;
}

FPathChallenge APlayerAIController::CheckForDropToTarget(const FVector& DirectionToTarget)
{
    FPathChallenge Challenge;

    if (!ControlledCharacter || DirectionToTarget.IsZero()) return Challenge;

    FVector CurrentLocation = ControlledCharacter->GetActorLocation();

    // Check at multiple distances toward the target
    TArray<float> CheckDistances = { 30.f, 60.f, 100.f, 150.f };

    for (float Distance : CheckDistances)
    {
        FVector TestPoint = CurrentLocation + DirectionToTarget * Distance;

        LogState(FString::Printf(TEXT("CheckForDropToTarget: Testing at %.1f units, pos: %s"),
            Distance, *TestPoint.ToString()), FColor::Cyan);

        if (!IsOnNavMesh(TestPoint))
        {
            LogState(TEXT("Found edge toward target - checking for safe drop"), FColor::Yellow);

            FVector EdgePosition = FindActualEdgePosition(CurrentLocation, TestPoint);

            // Check for ground below the edge
            FVector GroundLocation;
            if (HasGroundAtLocation(EdgePosition, MaxSafeDropHeight, &GroundLocation))
            {
                float DropDistance = EdgePosition.Z - GroundLocation.Z;
                if (DropDistance > MinDropHeight && DropDistance < MaxSafeDropHeight)
                {
                    if (HasNavMeshAtLocation(GroundLocation))
                    {
                        // Check if dropping here gets us closer to the target
                        float CurrentDistanceToTarget = FVector::Dist(CurrentLocation, CurrentTarget);
                        float DropDistanceToTarget = FVector::Dist(GroundLocation, CurrentTarget);

                        if (DropDistanceToTarget < CurrentDistanceToTarget)
                        {
                            LogState(FString::Printf(TEXT("Safe drop found toward target! Drop: %.1f units"), DropDistance), FColor::Green);

                            Challenge.Position = EdgePosition;
                            Challenge.RequiredAction = EManeuverType::Drop;
                            Challenge.DistanceFromStart = Distance;
                            Challenge.Description = FString::Printf(TEXT("Drop to target (%.1f units)"), DropDistance);
                            return Challenge;
                        }
                    }
                }
            }

            LogState(TEXT("Edge found but drop not safe"), FColor::Orange);
            break;
        }
    }

    LogState(TEXT("No drop opportunity toward target found"), FColor::Black);
    return Challenge;
}

bool APlayerAIController::TryEngageNearbyEnemy() {
    if (!ControlledCharacter || !ControlledCharacter->GetEquippedWeapon()) {
        ClearCombatTarget();
        return false;
    }

    if (ControlledCharacter->GetEquippedWeapon()->BulletsInMag <= 0) {
        ClearCombatTarget();
        return false;
    }

    FVector PlayerLoc = ControlledCharacter->GetActorLocation();
    float CurrentTime = GetWorld()->GetTimeSeconds();

    if (CurrentTime - LastEnemyUpdateTime < EnemyUpdateInterval && CurrentTargetEnemy) {
        return UpdateCombatAiming();
    }

    LastEnemyUpdateTime = CurrentTime;

    // Find the closest valid enemy
    AFPSEnemyBase* BestEnemy = nullptr;
    float ClosestDistance = CombatRadius;

    for (TActorIterator<AFPSEnemyBase> It(GetWorld()); It; ++It) {
        AFPSEnemyBase* Enemy = *It;
        if (!Enemy || Enemy->IsPendingKillPending()) continue;

        float Distance = FVector::Dist(PlayerLoc, Enemy->GetActorLocation());
        if (Distance < ClosestDistance) {
            if (HasLineOfSightToEnemy(Enemy)) {
                BestEnemy = Enemy;
                ClosestDistance = Distance;
            }
        }
    }

    // Update target
    if (BestEnemy != CurrentTargetEnemy) {
        CurrentTargetEnemy = BestEnemy;

        if (CurrentTargetEnemy) {
            LogState(FString::Printf(TEXT("New combat target: %s at distance %.1f"),
                *CurrentTargetEnemy->GetName(), ClosestDistance), FColor::Red);
        }
        else {
            LogState(TEXT("No valid combat target found"), FColor::Yellow);
        }
    }

    if (CurrentTargetEnemy) {
        bShouldEngageEnemy = true;
        bFoundTarget = true;
        return UpdateCombatAiming();
    }
    else {
        ClearCombatTarget();
        return false;
    }
}

bool APlayerAIController::UpdateCombatAiming() {
    if (!CurrentTargetEnemy || !ControlledCharacter) {
        return false;
    }

    if (CurrentTargetEnemy->IsPendingKillPending()) {
        ClearCombatTarget();
        return false;
    }

    FVector PlayerLoc = ControlledCharacter->GetActorLocation();
    FVector EnemyLoc = CurrentTargetEnemy->GetActorLocation();
    float Distance = FVector::Dist(PlayerLoc, EnemyLoc);

    if (Distance > CombatRadius) {
        ClearCombatTarget();
        return false;
    }

    // Calculate aim direction
    FVector AimTarget = EnemyLoc + FVector(0, 0, 50);
    FVector AimDirection = (AimTarget - PlayerLoc).GetSafeNormal();

    FRotator CurrentControlRotation = ControlledCharacter->GetControlRotation();
    FRotator TargetRotation = AimDirection.Rotation();

    FRotator DeltaRotation = TargetRotation - CurrentControlRotation;

    DeltaRotation.Normalize();

    float MaxRotationSpeed = AimingSpeed * GetWorld()->GetDeltaSeconds() * 57.2958f; // Convert to degrees

    float ClampedYaw = FMath::Clamp(DeltaRotation.Yaw, -MaxRotationSpeed, MaxRotationSpeed);
    float ClampedPitch = FMath::Clamp(DeltaRotation.Pitch, -MaxRotationSpeed, MaxRotationSpeed);

    // Apply the rotation input
    if (FMath::Abs(ClampedYaw) > 0.1f)
    {
        FRotator CurrentRotation = ControlledCharacter->GetActorRotation();
        FRotator DesiredRotation = (AimTarget - PlayerLoc).Rotation();
        DesiredRotation.Pitch = CurrentRotation.Pitch;

        float RotationSpeed = 500.f;
        FRotator NewRotation = FMath::RInterpTo(CurrentRotation, DesiredRotation, GetWorld()->GetDeltaSeconds(), RotationSpeed / 180.f);

        ControlledCharacter->SetActorRotation(NewRotation);
    }

    // Check if we're aimed close enough to start/continue firing
    bool bIsAimedAtTarget = (FMath::Abs(DeltaRotation.Yaw) < AimTolerance &&
        FMath::Abs(DeltaRotation.Pitch) < AimTolerance);

    if (bIsAimedAtTarget) {
        ControlledCharacter->StartFire();
        LogState(TEXT("Firing at target"), FColor::Green);
    }
    else {
        ControlledCharacter->StopFire();
        LogState(FString::Printf(TEXT("Aiming: Yaw=%.1f, Pitch=%.1f"),
            DeltaRotation.Yaw, DeltaRotation.Pitch), FColor::Orange);
    }

    return true;
}

void APlayerAIController::ClearCombatTarget() {
    if (CurrentTargetEnemy) {
        LogState(TEXT("Clearing combat target"), FColor::Yellow);
    }

    CurrentTargetEnemy = nullptr;
    bShouldEngageEnemy = false;
    bFoundTarget = false;

    if (ControlledCharacter) {
        ControlledCharacter->StopFire();
    }
}

bool APlayerAIController::HasLineOfSightToEnemy(AFPSEnemyBase* Enemy) {
    if (!Enemy || !ControlledCharacter) return false;

    FVector StartLoc = ControlledCharacter->GetActorLocation() + FVector(0, 0, 50); // Eye level
    FVector EndLoc = Enemy->GetActorLocation() + FVector(0, 0, 50); // Enemy center

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(ControlledCharacter);
    Params.AddIgnoredActor(Enemy);

    FHitResult Hit;
    bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, StartLoc, EndLoc, ECC_Visibility, Params);

    return !bHit;
}
