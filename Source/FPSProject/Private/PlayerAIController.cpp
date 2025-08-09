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

    CurrentTarget = NewTarget;
    bHasTarget = true;
    CurrentIntent = ENavigationIntent::Following;

    // Interrupt any current maneuver
    if (CurrentIntent == ENavigationIntent::ExecutingManeuver)
    {
        CompleteManeuver(false);
    }

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

    // Update tracking variables
    FVector CurrentLocation = ControlledCharacter->GetActorLocation();
    bool bOnNavMesh = IsOnNavMesh(CurrentLocation);

    // ALWAYS try to engage enemies, regardless of other states
    // This ensures continuous aiming updates
    bool bEngagingEnemy = TryEngageNearbyEnemy();

    if (bOnNavMesh)
    {
        LastKnownNavMeshPosition = CurrentLocation;
        LastValidPosition = CurrentLocation;
    }

    // Check for stuck condition (but not if we're in combat)
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

        // Handle emergency conditions
        if (StuckTimer > StuckThreshold && CurrentIntent != ENavigationIntent::EmergencyRecovery)
        {
            //LogState(TEXT("STUCK! Starting emergency recovery"), FColor::Red);
            StartEmergencyRecovery();
        }
    }
    else {
        // Reset stuck timer when in combat
        StuckTimer = 0.f;
    }

    // State-based processing (skip if engaging enemy)
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
            // Do nothing
            break;
        }
    }

    bWasOnNavMeshLastFrame = bOnNavMesh;

    // Debug visualization
    if (bDebugVisualization)
    {
        DrawDebugInfo();
    }
}

// === CORE NAVIGATION ===

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

                // Before regenerating path, check if we need to drop
                FVector DirectionToTarget = (CurrentTarget - CurrentLocation).GetSafeNormal2D();

                // Check if there's a drop opportunity toward the target
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

                // If no drop opportunity, try normal path regeneration
                UpdatePath();
            }
        }
        return;
    }

    FVector NextPoint = PathPoints[CurrentPathIndex];
    FVector CurrentLocation = ControlledCharacter->GetActorLocation();
    FVector DirectionToWaypoint = (NextPoint - CurrentLocation).GetSafeNormal2D();

    // Smoothly rotate character toward the waypoint direction
    if (!DirectionToWaypoint.IsZero())
    {
        FRotator TargetRotation = DirectionToWaypoint.Rotation();
        FRotator CurrentRotation = ControlledCharacter->GetActorRotation();

        // Only rotate around Z-axis (yaw)
        TargetRotation.Pitch = CurrentRotation.Pitch;
        TargetRotation.Roll = CurrentRotation.Roll;

        // Smoothly interpolate to target rotation
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

    // Look ahead for challenges - but look toward the next waypoint, not forward
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

    // Normal movement toward next waypoint
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
        if (CrouchTimer > 1.f) // Try to stand up periodically
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

// === PREDICTIVE PLANNING ===

FPathChallenge APlayerAIController::AnalyzePathAhead(float LookaheadDistance, const FVector& MovementDirection)
{
    FPathChallenge Challenge;

    if (!ControlledCharacter) return Challenge;

    FVector CurrentLocation = ControlledCharacter->GetActorLocation();
    FVector ActualForwardVector = ControlledCharacter->GetActorForwardVector();

    // Use movement direction for drops, but check obstacles in multiple directions
    FVector MovementDir = MovementDirection.IsZero() ? ActualForwardVector : MovementDirection;

    //LogState(FString::Printf(TEXT("AnalyzePathAhead: Movement=%s, Forward=%s"),
    //    *MovementDir.ToString(), *ActualForwardVector.ToString()), FColor::Black);

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(ControlledCharacter);
    const UWorld* World = GetWorld();
    float CapsuleHalfHeight = ControlledCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

    // 1. First, check for obstacles ONLY in the movement direction
    // This prevents detecting irrelevant side obstacles when there's a clear path ahead
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

    // 2. Check for crouch obstacles in movement direction
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

    // 3. Only if there's no clear path in movement direction, check other directions for alternative obstacles
    // This handles cases where character facing doesn't match movement (like your original crouch issue)
    if (!bObstacleInMovementPath && !MovementDir.Equals(ActualForwardVector, 0.1f))
    {
        //LogState(TEXT("No obstacle in movement path, checking facing direction for environmental hazards"), FColor::Black);

        // Check facing direction for crouch obstacles (most common case)
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

    // 3. Check for drop opportunities ONLY in the movement direction
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
                        // Additional check: make sure this drop makes sense contextually
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
        return true; // Crouch is generally safe

    default:
        return false;
    }
}

// === MANEUVER EXECUTION ===

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
        // For drops, we'll force falling mode and move forward
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

    // Timeout check
    if (ManeuverTimer > CurrentManeuver.ExpectedDuration || ManeuverTimer > MaxManeuverDuration)
    {
        //LogState(TEXT("Maneuver timed out"), FColor::Orange);
        CompleteManeuver(false);
        return;
    }

    // Continue movement in planned direction
    if (!CurrentManeuver.MovementDirection.IsZero())
    {
        MoveInWorldDirection(CurrentManeuver.MovementDirection);
    }

    // Check for completion conditions
    FVector CurrentLocation = ControlledCharacter->GetActorLocation();

    switch (CurrentManeuver.Type)
    {
    case EManeuverType::Jump:
    case EManeuverType::Drop:
        // Wait for landing callback
        if (!bWaitingForLanding)
        {
            // Add a small delay after landing to ensure we're stable
            if (ManeuverTimer > 0.5f) // Wait at least 0.5 seconds after landing
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
            // Still waiting for landing - check if we've been falling too long
            if (ManeuverTimer > 3.0f) // If falling for more than 3 seconds, something's wrong
            {
               // LogState(TEXT("Maneuver taking too long - forcing completion"), FColor::Red);
                bWaitingForLanding = false;
                CompleteManeuver(false);
            }
        }
        break;

    case EManeuverType::Crouch:
        // Check if we've passed the obstacle
        if (ManeuverTimer > 1.f) // Give some time for crouching
        {
            FVector Direction = (CurrentManeuver.TargetPosition - CurrentLocation).GetSafeNormal2D();
            float DistanceToTarget = FVector::Dist2D(CurrentLocation, CurrentManeuver.TargetPosition);

            if (DistanceToTarget < 50.f)
            {
                CompleteManeuver(true);
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

        // Recalculate path from new position
        UpdatePath();
    }
    else
    {
      // LogState(FString::Printf(TEXT("Maneuver failed: %d"), (int32)CompletedType), FColor::Red);
        StartEmergencyRecovery();
    }
}

// === OBSTACLE ANALYSIS ===

bool APlayerAIController::AnalyzeJumpObstacle(const FVector& ObstacleLocation, FManeuverPlan& OutPlan)
{
    FVector CurrentLocation = ControlledCharacter->GetActorLocation();
    FVector ForwardVector = (ObstacleLocation - CurrentLocation).GetSafeNormal2D();

    // Predict landing location
    FVector LandingCheckStart = CurrentLocation + ForwardVector * 200.f;
    LandingCheckStart.Z = CurrentLocation.Z + 50.f; // Jump height

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
    // For crouch obstacles, target is past the obstacle
    FVector Direction = (ObstacleLocation - ControlledCharacter->GetActorLocation()).GetSafeNormal2D();
    OutPlan.TargetPosition = ObstacleLocation + Direction * 100.f;
    OutPlan.MovementDirection = Direction;
    OutPlan.bTargetIsOnNavMesh = true;
    return true;
}

// === SAFETY CHECKS ===

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

// === EMERGENCY RECOVERY ===

void APlayerAIController::StartEmergencyRecovery()
{
    CurrentIntent = ENavigationIntent::EmergencyRecovery;
    RecoveryTimer = 0.f;
    RecoveryAttempts = 0;
    TriedRecoveryDirections.Empty();
    StuckTimer = 0.f;

    // Cancel any ongoing maneuver
    if (CurrentManeuver.IsValid())
    {
        CompleteManeuver(false);
    }

   // LogState(TEXT("EMERGENCY RECOVERY STARTED"), FColor::Red);
}

void APlayerAIController::HandleEmergencyRecovery(float DeltaTime)
{
    RecoveryTimer += DeltaTime;

    // Check if we're back on navmesh
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

    // Timeout check
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
        // No good direction found - try random movement
        RecoveryAttempts++;
        if (RecoveryAttempts > 30)
        {
       //     LogState(TEXT("Too many recovery attempts - giving up"), FColor::Red);
            ClearTarget();
            return;
        }

        // Generate a semi-random direction that we haven't tried
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

    // Try directions toward target, last known navmesh, and cardinal directions
    TArray<FVector> TestDirections;

    // Priority 1: Direction toward target
    if (bHasTarget)
    {
        FVector ToTarget = (CurrentTarget - CurrentLocation).GetSafeNormal2D();
        if (!ToTarget.IsZero()) TestDirections.Add(ToTarget);
    }

    // Priority 2: Direction toward last known navmesh position
    if (!LastKnownNavMeshPosition.IsZero())
    {
        FVector ToLastKnown = (LastKnownNavMeshPosition - CurrentLocation).GetSafeNormal2D();
        if (!ToLastKnown.IsZero()) TestDirections.Add(ToLastKnown);
    }

    // Priority 3: Cardinal and diagonal directions
    TArray<FVector> CardinalDirections = {
        FVector(1, 0, 0), FVector(-1, 0, 0), FVector(0, 1, 0), FVector(0, -1, 0),
        FVector(0.707f, 0.707f, 0), FVector(-0.707f, 0.707f, 0),
        FVector(0.707f, -0.707f, 0), FVector(-0.707f, -0.707f, 0)
    };

    TestDirections.Append(CardinalDirections);

    // Test each direction
    for (const FVector& Direction : TestDirections)
    {
        // Skip directions we've already tried recently
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
                // Found navmesh - make sure it's actually reachable
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

// === UTILITY ===

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

// === EVENT HANDLERS ===

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

    // Reset the maneuver timer when we land so the completion logic can work properly
    if (CurrentIntent == ENavigationIntent::ExecutingManeuver)
    {
        ManeuverTimer = 0.f; // Reset timer so the delay check starts from landing
     //   LogState(TEXT("Maneuver timer reset on landing"), FColor::Green);
    }

    // If we weren't expecting to land, this might be from emergency recovery
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

    // If we started falling unexpectedly, we might need recovery
    if (NewMode == MOVE_Falling && PrevMovementMode == MOVE_Walking)
    {
        if (CurrentIntent == ENavigationIntent::Following)
        {
            // Unexpected fall - start tracking for potential recovery
            StuckTimer = 0.f; // Reset stuck timer since falling is movement
        }
    }
}

// === DEBUG ===

void APlayerAIController::DrawDebugInfo()
{
    if (!ControlledCharacter) return;

    const UWorld* World = GetWorld();
    if (!World) return;

    FVector CurrentLocation = ControlledCharacter->GetActorLocation();

    // Draw current path
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

    // Draw target
    if (bHasTarget)
    {
        DrawDebugSphere(World, CurrentTarget, 50.f, 8, FColor::Red, false, -1.f, 0, 3.f);
        DrawDebugLine(World, CurrentLocation, CurrentTarget, FColor::Orange, false, -1.f, 0, 1.f);
    }

    // Draw current maneuver info
    if (CurrentManeuver.IsValid())
    {
        DrawDebugSphere(World, CurrentManeuver.TargetPosition, 30.f, 8, FColor::Magenta, false, -1.f, 0, 2.f);
        DrawDebugLine(World, CurrentLocation, CurrentManeuver.TargetPosition, FColor::Magenta, false, -1.f, 0, 2.f);
    }

    // Draw last known navmesh position
    if (!LastKnownNavMeshPosition.IsZero())
    {
        DrawDebugSphere(World, LastKnownNavMeshPosition, 20.f, 8, FColor::Cyan, false, -1.f, 0, 1.f);
    }

    // Status text
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

            // Trace down to find the ground at the edge
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

            // Found an edge - get the actual edge position
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

            // Found edge but no safe drop
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

    // Don't update enemy targeting too frequently for performance
    if (CurrentTime - LastEnemyUpdateTime < EnemyUpdateInterval && CurrentTargetEnemy) {
        // Just update aiming for current target
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

    // Check if enemy is still valid and in range
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

    // Calculate aim direction - aim slightly above center mass
    FVector AimTarget = EnemyLoc + FVector(0, 0, 50);
    FVector AimDirection = (AimTarget - PlayerLoc).GetSafeNormal();

    // Get current and target rotations
    FRotator CurrentControlRotation = ControlledCharacter->GetControlRotation();
    FRotator TargetRotation = AimDirection.Rotation();

    // Calculate rotation difference
    FRotator DeltaRotation = TargetRotation - CurrentControlRotation;

    // Normalize the rotation difference to [-180, 180] range
    DeltaRotation.Normalize();

    // Apply aiming with speed limiting for smoother tracking
    float MaxRotationSpeed = AimingSpeed * GetWorld()->GetDeltaSeconds() * 57.2958f; // Convert to degrees

    float ClampedYaw = FMath::Clamp(DeltaRotation.Yaw, -MaxRotationSpeed, MaxRotationSpeed);
    float ClampedPitch = FMath::Clamp(DeltaRotation.Pitch, -MaxRotationSpeed, MaxRotationSpeed);

    // Apply the rotation input
    if (FMath::Abs(ClampedYaw) > 0.1f)
    {
        FRotator CurrentRotation = ControlledCharacter->GetActorRotation();
        FRotator DesiredRotation = (AimTarget - PlayerLoc).Rotation();
        DesiredRotation.Pitch = CurrentRotation.Pitch; // Ignore pitch unless you want the mesh to tilt

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

    // If we hit something, we don't have line of sight
    return !bHit;
}
