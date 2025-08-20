#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "PauseMenuWidget.generated.h"

UCLASS()
class FPSPROJECT_API UPauseMenuWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;

public:
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    UButton* Resume;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    UButton* QuitMenu;
};
