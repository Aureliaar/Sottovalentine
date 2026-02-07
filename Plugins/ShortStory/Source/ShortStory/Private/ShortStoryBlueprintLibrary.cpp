// Copyright Theory of Magic. All Rights Reserved.

#include "ShortStoryBlueprintLibrary.h"
#include "ShortStoryParser.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Engine.h"

float UShortStoryBlueprintLibrary::GetPauseDurationSeconds(EStoryPauseDuration Pause)
{
	switch (Pause)
	{
	case EStoryPauseDuration::None:
		return 0.0f;
	case EStoryPauseDuration::Short:
		return 0.5f;
	case EStoryPauseDuration::Standard:
		return 1.0f;
	case EStoryPauseDuration::Long:
		return 2.0f;
	default:
		return 0.0f;
	}
}

TArray<FVector2D> UShortStoryBlueprintLibrary::CalculateWordPositions(const FString& Text, EStoryLineAnimation AnimType, FVector2D CanvasSize, float FontSize)
{
	TArray<FVector2D> Positions;

	// Split text into words
	TArray<FString> Words = SplitTextIntoWords(Text);
	if (Words.Num() == 0)
	{
		return Positions;
	}

	// Calculate approximate word spacing
	const float CharWidth = FontSize * 0.6f; // Approximate monospace character width
	const float SpaceWidth = FontSize * 0.3f;
	const float LineHeight = FontSize * 1.5f;

	// Center positions
	const FVector2D Center = CanvasSize * 0.5f;

	switch (AnimType)
	{
	case EStoryLineAnimation::Typewriter:
	case EStoryLineAnimation::Paragraph:
		{
			// Left-aligned, vertically centered
			float CurrentX = CanvasSize.X * 0.1f; // 10% from left
			float CurrentY = Center.Y;

			for (const FString& Word : Words)
			{
				float WordWidth = Word.Len() * CharWidth;

				// Check if we need to wrap to next line
				if (CurrentX + WordWidth > CanvasSize.X * 0.9f && Positions.Num() > 0)
				{
					CurrentX = CanvasSize.X * 0.1f;
					CurrentY += LineHeight;
				}

				Positions.Add(FVector2D(CurrentX, CurrentY));
				CurrentX += WordWidth + SpaceWidth;
			}
		}
		break;

	case EStoryLineAnimation::LeftToRight:
		{
			// Animate from left to center
			float TotalWidth = 0.0f;
			for (const FString& Word : Words)
			{
				TotalWidth += Word.Len() * CharWidth + SpaceWidth;
			}

			float StartX = Center.X - TotalWidth * 0.5f;
			float CurrentX = StartX;
			float Y = Center.Y;

			for (const FString& Word : Words)
			{
				Positions.Add(FVector2D(CurrentX, Y));
				CurrentX += Word.Len() * CharWidth + SpaceWidth;
			}
		}
		break;

	case EStoryLineAnimation::TopDown:
		{
			// Vertically stacked, centered
			float StartY = Center.Y - (Words.Num() * LineHeight * 0.5f);

			for (int32 i = 0; i < Words.Num(); ++i)
			{
				Positions.Add(FVector2D(Center.X, StartY + i * LineHeight));
			}
		}
		break;

	case EStoryLineAnimation::WordRain:
		{
			// Random X positions, words "fall" from top
			for (int32 i = 0; i < Words.Num(); ++i)
			{
				// Random X position within canvas bounds (with margins)
				float RandomX = FMath::FRandRange(CanvasSize.X * 0.1f, CanvasSize.X * 0.9f);

				// Start above canvas, final position in center region
				float FinalY = Center.Y + FMath::FRandRange(-LineHeight * 2, LineHeight * 2);

				Positions.Add(FVector2D(RandomX, FinalY));
			}
		}
		break;

	case EStoryLineAnimation::Snake:
		{
			// Wavy serpentine path
			const float Amplitude = LineHeight * 2.0f;
			const float Frequency = 0.5f;
			const float WordSpacing = FontSize * 3.0f;

			float StartX = CanvasSize.X * 0.1f;
			float BaseY = Center.Y;

			for (int32 i = 0; i < Words.Num(); ++i)
			{
				float X = StartX + i * WordSpacing;
				float Y = BaseY + Amplitude * FMath::Sin(i * Frequency);

				// Wrap to next line if needed
				if (X > CanvasSize.X * 0.9f)
				{
					X = StartX;
					BaseY += LineHeight * 3.0f;
					Y = BaseY + Amplitude * FMath::Sin(i * Frequency);
				}

				Positions.Add(FVector2D(X, Y));
			}
		}
		break;

	default:
		// Fallback to standard positioning
		Positions.Add(Center);
		break;
	}

	return Positions;
}

void UShortStoryBlueprintLibrary::TriggerStoryEffect(UObject* WorldContextObject, EStoryEffect Effect, int32 PlayerIndex)
{
	if (!WorldContextObject)
	{
		return;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		return;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(World, PlayerIndex);
	if (!PC)
	{
		return;
	}

	float Intensity = GetScreenShakeIntensity(Effect);

	if (Intensity > 0.0f)
	{
		// Simple camera shake using PlayerCameraManager
		if (APlayerCameraManager* CameraManager = PC->PlayerCameraManager)
		{
			// Scale shake parameters by intensity
			float ShakeScale = Intensity;
			float Duration = 0.5f + (Intensity * 0.5f); // 0.5s to 1.0s
			float Frequency = 10.0f + (Intensity * 20.0f); // 10 to 30 Hz

			// Apply simple camera shake (using FOV modulation as fallback)
			// In a real implementation, you'd use UCameraShakeBase here
			UE_LOG(LogShortStory, Log, TEXT("TriggerStoryEffect: Shake intensity %.2f"), Intensity);
		}
	}

	// Special effects
	switch (Effect)
	{
	case EStoryEffect::Storm:
		{
			// TODO: Spawn lightning/thunder VFX
			UE_LOG(LogShortStory, Log, TEXT("TriggerStoryEffect: Storm effect triggered"));
		}
		break;

	default:
		break;
	}
}

float UShortStoryBlueprintLibrary::GetScreenShakeIntensity(EStoryEffect Effect)
{
	switch (Effect)
	{
	case EStoryEffect::ShakeLow:
		return 0.3f;
	case EStoryEffect::ShakeMed:
		return 0.6f;
	case EStoryEffect::ShakeHigh:
		return 1.0f;
	case EStoryEffect::Storm:
		return 0.8f; // Storm includes shake
	case EStoryEffect::None:
	default:
		return 0.0f;
	}
}

TArray<FString> UShortStoryBlueprintLibrary::SplitTextIntoWords(const FString& Text)
{
	TArray<FString> Words;

	// Split by spaces, but preserve punctuation with words
	FString CurrentWord;
	for (int32 i = 0; i < Text.Len(); ++i)
	{
		TCHAR Char = Text[i];

		if (FChar::IsWhitespace(Char))
		{
			if (!CurrentWord.IsEmpty())
			{
				Words.Add(CurrentWord);
				CurrentWord.Empty();
			}
		}
		else
		{
			CurrentWord.AppendChar(Char);
		}
	}

	// Add last word
	if (!CurrentWord.IsEmpty())
	{
		Words.Add(CurrentWord);
	}

	return Words;
}

float UShortStoryBlueprintLibrary::GetAnimationDuration(EStoryLineAnimation AnimType, int32 WordCount)
{
	const float BaseWordTime = 0.15f; // Base time per word

	switch (AnimType)
	{
	case EStoryLineAnimation::Typewriter:
		return 0.5f; // Quick appearance
	case EStoryLineAnimation::LeftToRight:
		return FMath::Max(1.5f, WordCount * 0.1f);
	case EStoryLineAnimation::TopDown:
		return FMath::Max(2.0f, WordCount * 0.2f);
	case EStoryLineAnimation::WordRain:
		return FMath::Max(2.5f, WordCount * 0.15f);
	case EStoryLineAnimation::Snake:
		return FMath::Max(2.0f, WordCount * 0.2f);

	case EStoryLineAnimation::Paragraph:
		return 0.3f; // All at once, quick fade
	default:
		return 1.0f;
	}
}

float UShortStoryBlueprintLibrary::GetAnimationDurationForLine(const FStoryLine& Line)
{
	TArray<FString> Words = SplitTextIntoWords(Line.Text);
	return GetAnimationDuration(Line.AnimationType, Words.Num());
}

float UShortStoryBlueprintLibrary::GetPauseDurationSecondsForLine(const FStoryLine& Line)
{
	return GetPauseDurationSeconds(Line.PauseDuration);
}

FVector2D UShortStoryBlueprintLibrary::CalculateCenteredPosition(FVector2D CanvasSize, FVector2D TextSize)
{
	return (CanvasSize - TextSize) * 0.5f;
}

float UShortStoryBlueprintLibrary::CalculateLineVerticalPosition(int32 LineIndex, int32 TotalLines, FVector2D CanvasSize, float LineHeight)
{
	if (TotalLines <= 0)
	{
		return CanvasSize.Y * 0.5f;
	}

	// Calculate total height of all lines
	float TotalHeight = TotalLines * LineHeight;

	// Start Y position (top of text block, centered vertically)
	float StartY = (CanvasSize.Y - TotalHeight) * 0.5f;

	// Position for this specific line (center of line)
	return StartY + (LineIndex * LineHeight) + (LineHeight * 0.5f);
}
