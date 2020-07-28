// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "UI/TouchControls.h"

#include "Blueprint/SlateBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Controllers/Components/ControllerEventsComponent.h"
#include "Controllers/GDKPlayerController.h"
#include "Framework/Application/SlateApplication.h"

DEFINE_LOG_CATEGORY(LogTouchControls);

const float MaxAvailableAngle = -PI / 4;
const float MinAvailableAngle = -3 * PI / 4;

bool FORCEINLINE InLeftControllerResponseArea(const FVector2D& LocalPosition, const FVector2D& ScreenSize)
{
	return LocalPosition.X > 0 && LocalPosition.X < ScreenSize.X / 2 && LocalPosition.Y > 0 && LocalPosition.Y < ScreenSize.Y;
}

UTouchControls::UTouchControls(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer), bControlsBound(false) {}

void UTouchControls::NativeConstruct()
{
	Super::NativeConstruct();

	APlayerController* PlayerController = GetOwningPlayer();
	if (PlayerController)
	{
		PlayerController->InputYawScale = 1.0f;
		PlayerController->InputPitchScale = -1.0f;
	}

	// Get the left controller center position through the fore icon in blueprint.
	LeftForeImageCanvasSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(LeftControllerForeImage);
	LeftControllerInfo.LeftControllerCenter = LeftForeImageCanvasSlot->GetPosition();

	// Get the radius of left controller through the back icon in blueprint.
	UCanvasPanelSlot* LeftBackImageCanvasSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(LeftControllerBackImage);
	LeftControllerInfo.DistanceToEdge = LeftBackImageCanvasSlot->GetSize().X / 2;
	LeftControllerInfo.DistanceToEdgeSquare = FMath::Square(LeftControllerInfo.DistanceToEdge);

	//SprintWidget->SetVisibility(ESlateVisibility::Hidden);
	//SprintArrow->SetVisibility(ESlateVisibility::Hidden);

	// Bind keep sprint icon touch event to KeepSprintAction function.
	//SprintWidget->OnMouseButtonUpEvent.BindDynamic(this, &UTouchControls::KeepSprintAction);
	if (UControllerEventsComponent* ControllerEvents = Cast<UControllerEventsComponent>(PlayerController->GetComponentByClass(UControllerEventsComponent::StaticClass())))
	{
		ControllerEvents->DeathDetailsEvent.AddDynamic(this, &UTouchControls::OnDeath);
	}
}

void UTouchControls::OnDeath(const FString& VictimName, int32 VictimId)
{
	SetVisibility(ESlateVisibility::Hidden);
}

void UTouchControls::BindControls()
{
	if (!bControlsBound)
	{
		JumpButton->OnMouseButtonDownEvent.BindDynamic(this, &UTouchControls::HandleJumpPressed);
		JumpButton->OnMouseButtonUpEvent.BindDynamic(this, &UTouchControls::HandleJumpReleased);

		CrouchSlideButton->OnMouseButtonDownEvent.BindDynamic(this, &UTouchControls::HandleCrouchPressed);
		CrouchSlideButton->OnMouseButtonUpEvent.BindDynamic(this, &UTouchControls::HandleCrouchReleased);

		RightShootButton->OnMouseButtonDownEvent.BindDynamic(this, &UTouchControls::HandleTriggerPressed);
		RightShootButton->OnMouseButtonUpEvent.BindDynamic(this, &UTouchControls::HandleTriggerReleased);

		SprintButton->OnMouseButtonDownEvent.BindDynamic(this, &UTouchControls::HandleSprintPressed);
		SprintButton->OnMouseButtonUpEvent.BindDynamic(this, &UTouchControls::HandleSprintReleased);

		SiteScopeButton->OnMouseButtonDownEvent.BindDynamic(this, &UTouchControls::HandleScopePressed);
		SiteScopeButton->OnMouseButtonUpEvent.BindDynamic(this, &UTouchControls::HandleScopeReleased);

		bControlsBound = true;
	}
}

FReply UTouchControls::NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	const uint32 TouchIndex = InGestureEvent.GetPointerIndex();
	const FVector2D& ScreenSize = InGeometry.GetLocalSize();
	const FVector2D LocalPosition = InGeometry.AbsoluteToLocal(InGestureEvent.GetScreenSpacePosition());
	UE_LOG(LogTouchControls, Verbose, TEXT("NativeOnTouchStarted, Pointer index: %d"), TouchIndex);

	if (InLeftControllerResponseArea(LocalPosition, ScreenSize))
	{
		LeftControllerInfo.LeftTouchStartPosition = LocalPosition;
		LeftControllerInfo.LeftControllerActive = true;
		LeftControllerInfo.TouchIndex = TouchIndex;
		return FReply::Handled();
	}
	else
	{
		if (!CameraTouch.active)
		{
			CameraTouch.LastPos = LocalPosition;
			CameraTouch.FingerIndex = TouchIndex;
			CameraTouch.active = true;
			return FReply::Handled();
		}
	}
	return FReply::Unhandled();
}

FReply UTouchControls::NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	const uint32 TouchIndex = InGestureEvent.GetPointerIndex();
	const FVector2D LocalPosition = InGeometry.AbsoluteToLocal(InGestureEvent.GetScreenSpacePosition());
	if (LeftControllerInfo.LeftControllerActive && LeftControllerInfo.TouchIndex == TouchIndex)
	{
		HandleTouchMoveOnLeftController(LocalPosition);
		return FReply::Handled();
	}
	else if (CameraTouch.active && CameraTouch.FingerIndex == TouchIndex)
	{
		HandleTouchMoveOnRightController(LocalPosition);
		return FReply::Handled();
	}
	else
	{
		return FReply::Unhandled();
	}
}

FReply UTouchControls::NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	const uint32 TouchIndex = InGestureEvent.GetPointerIndex();
	const FVector2D LocalPosition = InGeometry.AbsoluteToLocal(InGestureEvent.GetScreenSpacePosition());
	if (LeftControllerInfo.LeftControllerActive && LeftControllerInfo.TouchIndex == TouchIndex)
	{
		HandleTouchEndOnLeftController();
		return FReply::Handled();
	}
	else if (CameraTouch.active && CameraTouch.FingerIndex == TouchIndex)
	{
		HandleTouchEndOnRightController(LocalPosition);
		return FReply::Handled();
	}
	else
	{
		return FReply::Unhandled();
	}
}

void UTouchControls::NativeTick(const FGeometry& AllottedGeometry, float InDeltaTime)
{
	Super::NativeTick(AllottedGeometry, InDeltaTime);
	FSlateApplication::Get().OnControllerAnalog(FGamepadKeyNames::LeftAnalogX, 0, LeftControllerInfo.MoveVelocity.X);
	FSlateApplication::Get().OnControllerAnalog(FGamepadKeyNames::LeftAnalogY, 0, -LeftControllerInfo.MoveVelocity.Y);
}

void UTouchControls::ShowAllActionButtons(bool Enable) const
{
	const ESlateVisibility SlateVisibility = Enable ? ESlateVisibility::Visible : ESlateVisibility::Hidden;
	CrouchSlideButton->SetVisibility(SlateVisibility);
	JumpButton->SetVisibility(SlateVisibility);
	RightShootButton->SetVisibility(SlateVisibility);
	SiteScopeButton->SetVisibility(SlateVisibility);
	SprintButton->SetVisibility(SlateVisibility);
}

namespace
{
	static FEventReply HandlePressed(UWidget* Widget, FGamepadKeyNames::Type KeyName)
	{
		FEventReply Reply(FSlateApplication::Get().OnControllerButtonPressed(KeyName, 0, false));
		Reply.NativeReply.CaptureMouse(Widget->TakeWidget());
		return Reply;
	}

	static FEventReply HandleReleased(UTouchControls* Controls, FGamepadKeyNames::Type KeyName)
	{
		FEventReply Reply(FSlateApplication::Get().OnControllerButtonReleased(KeyName, 0, false));
		if (Reply.NativeReply.GetMouseCaptor().IsValid() == false && Controls->HasMouseCapture())
		{
			Reply.NativeReply.ReleaseMouseCapture();
		}
		return Reply;
	}

	static FEventReply HandleToggle(bool& bEnabled, UBorder* Button, FGamepadKeyNames::Type KeyName)
	{
		if (bEnabled)
		{
			Button->SetBrushColor(FLinearColor::White);
			FSlateApplication::Get().OnControllerButtonReleased(KeyName, 0, false);
		}
		else
		{
			Button->SetBrushColor(FLinearColor::Green);
			FSlateApplication::Get().OnControllerButtonPressed(KeyName, 0, false);
		}
		bEnabled = !bEnabled;
		return true;
	}
}

FEventReply UTouchControls::HandleJumpPressed(FGeometry Geometry, const FPointerEvent& MouseEvent)
{
	if (bCrouchPressed)
	{
		//if crouched release crouch then jump
		HandleCrouchPressed(Geometry, MouseEvent);
	}
	JumpButton->SetBrushColor(FLinearColor::Green);
	return HandlePressed(JumpButton, FGamepadKeyNames::FaceButtonBottom);
}

FEventReply UTouchControls::HandleJumpReleased(FGeometry Geometry, const FPointerEvent& MouseEvent)
{
	JumpButton->SetBrushColor(FLinearColor::White);
	return HandleReleased(this, FGamepadKeyNames::FaceButtonBottom);
}

FEventReply UTouchControls::HandleCrouchPressed(FGeometry Geometry, const FPointerEvent& MouseEvent)
{
	if (!bCrouchPressed)
	{
		if (bSprintPressed)
		{
			HandleSprintPressed(Geometry, MouseEvent);
		}
	}
	return HandleToggle(bCrouchPressed, CrouchSlideButton, FGamepadKeyNames::FaceButtonRight);
}

FEventReply UTouchControls::HandleCrouchReleased(FGeometry Geometry, const FPointerEvent& MouseEvent)
{
	return true;
}

FEventReply UTouchControls::HandleTriggerPressed(FGeometry Geometry, const FPointerEvent& MouseEvent)
{
	return HandlePressed(RightShootButton, FGamepadKeyNames::RightTriggerThreshold);
}

FEventReply UTouchControls::HandleTriggerReleased(FGeometry Geometry, const FPointerEvent& MouseEvent)
{
	return HandleReleased(this, FGamepadKeyNames::RightTriggerThreshold);
}

FEventReply UTouchControls::HandleScopePressed(FGeometry Geometry, const FPointerEvent& MouseEvent)
{
	if (!bScopePressed)
	{
		if (bSprintPressed)
		{
			//if sprint enabled, disable when using scope
			HandleSprintPressed(Geometry, MouseEvent);
		}
	}
	return HandleToggle(bScopePressed, SiteScopeButton, FGamepadKeyNames::LeftTriggerThreshold);
}

FEventReply UTouchControls::HandleScopeReleased(FGeometry Geometry, const FPointerEvent& MouseEvent)
{
	return true;
}

FEventReply UTouchControls::HandleSprintPressed(FGeometry Geometry, const FPointerEvent& MouseEvent)
{
	if (!bSprintPressed)
	{
		if (bCrouchPressed)
		{
			//if crouched release crouch then jump
			HandleCrouchPressed(Geometry, MouseEvent);
		}
		if (bScopePressed)
		{
			HandleScopePressed(Geometry, MouseEvent);
		}
	}
	return HandleToggle(bSprintPressed, SprintButton, FGamepadKeyNames::FaceButtonTop);
}

FEventReply UTouchControls::HandleSprintReleased(FGeometry Geometry, const FPointerEvent& MouseEvent)
{
	return true;
}

void UTouchControls::HandleTouchMoveOnLeftController(const FVector2D& TouchPosition)
{
	const FVector2D Offset = TouchPosition - LeftControllerInfo.LeftTouchStartPosition;

	const float DistanceToTouchStartSquare = Offset.SizeSquared();
	const float Angle = FMath::Atan2(Offset.Y, Offset.X);
	const float CosAngle = FMath::Cos(Angle);
	const float SinAngle = FMath::Sin(Angle);

	if (DistanceToTouchStartSquare > LeftControllerInfo.DistanceToEdgeSquare)
	{
		const float XOffset = LeftControllerInfo.DistanceToEdge * CosAngle;
		const float YOffset = LeftControllerInfo.DistanceToEdge * SinAngle;
		const FVector2D NewPosition =
			FVector2D(XOffset + LeftControllerInfo.LeftControllerCenter.X, YOffset + LeftControllerInfo.LeftControllerCenter.Y);
		LeftForeImageCanvasSlot->SetPosition(NewPosition);
	}
	else
	{
		LeftForeImageCanvasSlot->SetPosition(LeftControllerInfo.LeftControllerCenter + Offset);
	}

	LeftControllerInfo.MoveVelocity.X = CosAngle;
	LeftControllerInfo.MoveVelocity.Y = SinAngle;
}

void UTouchControls::HandleTouchEndOnLeftController()
{
	LeftControllerInfo.LeftControllerActive = false;
	LeftControllerInfo.TouchIndex = -1;
	LeftForeImageCanvasSlot->SetPosition(LeftControllerInfo.LeftControllerCenter);
	LeftControllerInfo.MoveVelocity = FVector2D::ZeroVector;
}

void UTouchControls::HandleTouchMoveOnRightController(const FVector2D& TouchPosition)
{
	const FVector2D currentPos = FVector2D(TouchPosition);

	float xdelta = currentPos.X - CameraTouch.LastPos.X;
	float ydelta = currentPos.Y - CameraTouch.LastPos.Y;
	xdelta = FMath::Clamp(xdelta, -10.0f, 10.0f) * SpeedStatisticsX.RequestDynamicScale(xdelta);
	ydelta = FMath::Clamp(ydelta, -10.0f, 10.0f) * SpeedStatisticsY.RequestDynamicScale(ydelta);

	APlayerController* Controller = GetOwningPlayer();
	if (Controller)
	{
		if (!FMath::IsNearlyZero(xdelta))
		{
			Controller->AddYawInput(xdelta);
		}
		if (!FMath::IsNearlyZero(ydelta))
		{
			Controller->AddPitchInput(ydelta);
		}
	}

	CameraTouch.LastPos = currentPos;
}

void UTouchControls::HandleTouchEndOnRightController(const FVector2D& TouchPosition)
{
	UE_LOG(LogTouchControls, Verbose, TEXT("Camera touch end"));
	CameraTouch.active = false;
	CameraTouch.FingerIndex = -1;
	SpeedStatisticsX.ClearData();
	SpeedStatisticsY.ClearData();
}

