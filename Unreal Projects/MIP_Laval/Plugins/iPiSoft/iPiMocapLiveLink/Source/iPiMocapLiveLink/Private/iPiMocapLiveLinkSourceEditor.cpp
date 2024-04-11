#include "iPiMocapLiveLinkSourceEditor.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "iPiMocapLiveLinkSourceEditor"

void iPiMocapLiveLinkSourceEditor::Construct(const FArguments& Args)
{
	OkClicked = Args._OnOkClicked;

	int32 defaultPort = 31455;

	ChildSlot
	[
		SNew(SBox)
		.WidthOverride(250)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.FillWidth(0.5f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("iPiMocapPortNumber", "Port Number"))
				]
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Fill)
				.FillWidth(0.5f)
				[
					SAssignNew(EditabledText, SEditableTextBox)
					.Text(FText::FromString(FString::FromInt(defaultPort)))
					.OnTextCommitted(this, &iPiMocapLiveLinkSourceEditor::OnEndpointChanged)
				]
			]
			+ SVerticalBox::Slot()
			.HAlign(HAlign_Right)
			.AutoHeight()
			[
				SNew(SButton)
				.OnClicked(this, &iPiMocapLiveLinkSourceEditor::OnOkClicked)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Ok", "Ok"))
				]
			]
		]
	];
}

void iPiMocapLiveLinkSourceEditor::OnEndpointChanged(const FText& NewValue, ETextCommit::Type)
{
	TSharedPtr<SEditableTextBox> EditabledTextPin = EditabledText.Pin();
	if (EditabledTextPin.IsValid())
	{
		uint16 port = FCString::Atoi(*NewValue.ToString());
		EditabledTextPin->SetText(FText::FromString(FString::FromInt(port)));
	}
}

FReply iPiMocapLiveLinkSourceEditor::OnOkClicked()
{
	TSharedPtr<SEditableTextBox> EditabledTextPin = EditabledText.Pin();
	if (EditabledTextPin.IsValid())
	{
		FIPv4Endpoint Endpoint;
		Endpoint.Address = FIPv4Address::Any;
		Endpoint.Port = FCString::Atoi(*(EditabledTextPin->GetText().ToString()));
		OkClicked.ExecuteIfBound(Endpoint);
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE