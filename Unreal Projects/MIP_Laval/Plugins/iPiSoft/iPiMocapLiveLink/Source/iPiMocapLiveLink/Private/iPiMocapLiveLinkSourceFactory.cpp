#include "iPiMocapLiveLinkSourceFactory.h"

#define LOCTEXT_NAMESPACE "UiPiMocapLiveLinkSourceFactory"

FText UiPiMocapLiveLinkSourceFactory::GetSourceDisplayName() const
{
	return LOCTEXT("SourceDisplayName", "iPi Mocap Live Link");
}

FText UiPiMocapLiveLinkSourceFactory::GetSourceTooltip() const
{
	return LOCTEXT("SourceTooltip", "Creates a connection to iPi Mocap UDP Stream");
}

TSharedPtr<SWidget> UiPiMocapLiveLinkSourceFactory::BuildCreationPanel(FOnLiveLinkSourceCreated InOnLiveLinkSourceCreated) const
{
	return SNew(iPiMocapLiveLinkSourceEditor)
		.OnOkClicked(iPiMocapLiveLinkSourceEditor::FOnOkClicked::CreateUObject(this, &UiPiMocapLiveLinkSourceFactory::OnOkClicked, InOnLiveLinkSourceCreated));
}

TSharedPtr<ILiveLinkSource> UiPiMocapLiveLinkSourceFactory::CreateSource(const FString& InConnectionString) const
{
	FIPv4Endpoint DeviceEndPoint;
	if (!FIPv4Endpoint::Parse(InConnectionString, DeviceEndPoint))
	{
		return TSharedPtr<ILiveLinkSource>();
	}

	return MakeShared<FiPiMocapLiveLinkSource>(DeviceEndPoint);
}

void UiPiMocapLiveLinkSourceFactory::OnOkClicked(FIPv4Endpoint InEndpoint, FOnLiveLinkSourceCreated InOnLiveLinkSourceCreated) const
{
	InOnLiveLinkSourceCreated.ExecuteIfBound(MakeShared<FiPiMocapLiveLinkSource>(InEndpoint), InEndpoint.ToString());
}

#undef LOCTEXT_NAMESPACE


