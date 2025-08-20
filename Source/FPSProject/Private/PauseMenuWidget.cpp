#include "PauseMenuWidget.h"
#include "Components/Button.h"
#include "Widgets/SWidget.h"
#include "Framework/MetaData/DriverMetaData.h"
#include "TimerManager.h"
#include "Engine/World.h"

TSharedRef<SWidget> UPauseMenuWidget::RebuildWidget()
{
    TSharedRef<SWidget> Root = Super::RebuildWidget();
    Root->AddMetadata(FDriverMetaData::Id(TEXT("PauseMenuRoot")));
    return Root;
}

void UPauseMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();

    GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
        {
            auto TagButton = [&](UButton* Btn, const TCHAR* Id)
                {
                    if (!Btn)
                    {
                        return;
                    }
                    TSharedPtr<SWidget> SlateWidget = Btn->GetCachedWidget();
                    if (SlateWidget.IsValid())
                    {
                        SlateWidget->AddMetadata(FDriverMetaData::Id(Id));
                    }
                };

            TagButton(Resume, TEXT("PauseMenu_ResumeBtn"));
            TagButton(QuitMenu, TEXT("PauseMenu_QuitBtn"));
        });
}
