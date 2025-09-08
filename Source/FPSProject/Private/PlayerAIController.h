#pragma once

#include "CoreMinimal.h"
#include "../FPSEnemyBase.h"
#include "AIController.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PlayerAIController.generated.h"

class AFPSCharacter;

UENUM(BlueprintType)
enum class ENavigationIntent : uint8
{
    Idle,              // Not moving
    Following,         // Normal pathfinding
    ExecutingManeuver, // Performing planned action
    EmergencyRecovery  // Only when truly stuck
};

UENUM(BlueprintType)
enum class EManeuverType : uint8
{
    None,
    Jump,
    Drop,
    Crouch,
    Walk
};

USTRUCT(BlueprintType)
struct FManeuverPlan
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    EManeuverType Type = EManeuverType::None;

    UPROPERTY(BlueprintReadOnly)
    FVector StartPosition = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly)
    FVector TargetPosition = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly)
    FVector MovementDirection = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly)
    float ExpectedDuration = 0.f;

    UPROPERTY(BlueprintReadOnly)
    bool bTargetIsOnNavMesh = false;

    UPROPERTY(BlueprintReadOnly)
    bool bRequiresOffNavMeshMovement = false;

    bool IsValid() const { return Type != EManeuverType::None; }
    void Reset() { *this = FManeuverPlan(); }
};

USTRUCT(BlueprintType)
struct FPathChallenge
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FVector Position = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly)
    EManeuverType RequiredAction = EManeuverType::Walk;

    UPROPERTY(BlueprintReadOnly)
    float DistanceFromStart = 0.f;

    UPROPERTY(BlueprintReadOnly)
    FString Description;

    bool IsValid() const { return !Position.IsZero(); }
};

UCLASS()
class APlayerAIController : public AAIController
{
    GENERATED_BODY()

public:
    APlayerAIController();

    UFUNCTION(BlueprintCallable)
    void SetTarget(const FVector& NewTarget);

    UFUNCTION(BlueprintCallable)
    void ClearTarget();

    UPROPERTY()
    bool bShouldEngageEnemy = false;

    UPROPERTY()
    bool bFoundTarget = false;

protected:
    virtual void OnPossess(APawn* InPawn) override;
    virtual void Tick(float DeltaTime) override;

    // === CORE NAVIGATION ===
    void UpdatePath();
    void ProcessNavigation(float DeltaTime);
    bool IsCloseToTarget(const FVector& Target, float Tolerance = 75.f) const;

    // === PREDICTIVE PLANNING ===
    FPathChallenge AnalyzePathAhead(float LookaheadDistance = 300.f, const FVector& AnalysisDirection = FVector::ZeroVector);
    FManeuverPlan PlanManeuver(const FPathChallenge& Challenge);
    bool ValidateManeuverPlan(const FManeuverPlan& Plan);

    // === MANEUVER EXECUTION ===
    void ExecuteManeuver(float DeltaTime);
    void StartManeuver(const FManeuverPlan& Plan);
    void CompleteManeuver(bool bSuccess);

    // === OBSTACLE ANALYSIS ===
    bool AnalyzeJumpObstacle(const FVector& ObstacleLocation, FManeuverPlan& OutPlan);
    bool AnalyzeDropOpportunity(const FVector& EdgeLocation, FManeuverPlan& OutPlan);
    bool AnalyzeCrouchObstacle(const FVector& ObstacleLocation, FManeuverPlan& OutPlan);

    // === SAFETY CHECKS ===
    bool IsJumpSafe(const FVector& StartPos, const FVector& LandingPos);
    bool IsDropSafe(const FVector& EdgePos, const FVector& LandingPos);
    bool HasNavMeshAtLocation(const FVector& Location, float Tolerance = 100.f) const;
    bool HasGroundAtLocation(const FVector& Location, float MaxDropDistance = 800.f, FVector* OutGroundLocation = nullptr) const;

    // === EMERGENCY RECOVERY ===
    void StartEmergencyRecovery();
    void HandleEmergencyRecovery(float DeltaTime);
    bool FindPathToNavMesh(FVector& OutDirection);

    // === UTILITY ===
    void MoveInWorldDirection(const FVector& Direction, float Speed = 1.f);
    bool IsOnNavMesh(const FVector& Location) const;
    FVector GetNextPathPoint() const;
    bool HasValidPath() const;
    FVector FindActualEdgePosition(const FVector& StartPos, const FVector& EndPos);
    FPathChallenge CheckForDropToTarget(const FVector& DirectionToTarget);

    // === EVENT HANDLERS ===
    UFUNCTION()
    void OnJumpLanded(const FHitResult& Hit);

    UFUNCTION()
    void OnMovementModeChanged(ACharacter* InCharacter, EMovementMode PrevMovementMode, uint8 PreviousCustomMode);

    UFUNCTION()
    bool TryEngageNearbyEnemy();

private:
    // === REFERENCES ===
    UPROPERTY()
    AFPSCharacter* ControlledCharacter = nullptr;

    // === NAVIGATION STATE ===
    UPROPERTY()
    UNavigationPath* CurrentNavPath = nullptr;

    UPROPERTY()
    TArray<FVector> PathPoints;

    int32 CurrentPathIndex = 0;
    bool bHasTarget = false;
    FVector CurrentTarget = FVector::ZeroVector;
    ENavigationIntent CurrentIntent = ENavigationIntent::Idle;

    // === MANEUVER STATE ===
    FManeuverPlan CurrentManeuver;
    float ManeuverStartTime = 0.f;
    float ManeuverTimer = 0.f;
    FVector ManeuverStartPosition = FVector::ZeroVector;
    bool bWaitingForLanding = false;

    // === RECOVERY STATE ===
    float RecoveryTimer = 0.f;
    int32 RecoveryAttempts = 0;
    FVector LastKnownNavMeshPosition = FVector::ZeroVector;
    TArray<FVector> TriedRecoveryDirections;

    // === MOVEMENT TRACKING ===
    FVector LastMovementDirection = FVector::ZeroVector;
    FVector LastValidPosition = FVector::ZeroVector;
    float StuckTimer = 0.f;
    bool bWasOnNavMeshLastFrame = true;

    // === CROUCH STATE ===
    bool bWasBlockedFromStanding = false;
    float CrouchTimer = 0.f;

    // === CONFIGURATION ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
    float PathLookaheadDistance = 300.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
    float MaxJumpHeight = 200.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
    float MaxSafeDropHeight = 800.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
    float MinDropHeight = 20.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
    float MaxManeuverDuration = 5.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
    float EmergencyRecoveryTimeout = 10.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
    float StuckThreshold = 2.f;

    UPROPERTY()
    float LastCrouchCheckTime = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
    bool bDebugVisualization = true;

    UPROPERTY()
    AFPSEnemyBase* CurrentTargetEnemy = nullptr;

    float LastEnemyUpdateTime = 0.f;
    float EnemyUpdateInterval = 0.1f; // Update enemy targeting 10 times per second

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
    float CombatRadius = 800.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
    float AimingSpeed = 1.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
    float AimTolerance = 5.0f;

    bool UpdateCombatAiming();
    void ClearCombatTarget();
    bool HasLineOfSightToEnemy(AFPSEnemyBase* Enemy);

    // === DEBUG ===
    void DrawDebugInfo();
    void LogState(const FString& Message, FColor Color = FColor::White);
};