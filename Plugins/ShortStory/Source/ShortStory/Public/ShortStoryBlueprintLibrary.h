// Copyright Theory of Magic. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ShortStoryStructs.h"
#include "ShortStoryBlueprintLibrary.generated.h"

/**
 * Blueprint function library for short story system
 * Provides helper functions for widgets and animation
 */
UCLASS()
class SHORTSTORY_API UShortStoryBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Get pause duration in seconds for a given pause type
	 * @param Pause Pause duration enum
	 * @return Duration in seconds
	 */
	UFUNCTION(BlueprintPure, Category = "Narrative|Short Stories")
	static float GetPauseDurationSeconds(EStoryPauseDuration Pause);

	/**
	 * Calculate word positions for animated text
	 * @param Text The text to position
	 * @param AnimType Animation type
	 * @param CanvasSize Size of the canvas (for bounds calculation)
	 * @param FontSize Font size (for spacing calculation)
	 * @return Array of positions for each word (screen-space coordinates)
	 */
	UFUNCTION(BlueprintPure, Category = "Narrative|Short Stories")
	static TArray<FVector2D> CalculateWordPositions(const FString& Text, EStoryLineAnimation AnimType, FVector2D CanvasSize, float FontSize = 24.0f);

	/**
	 * Trigger a story effect (screen shake, storm, etc.)
	 * @param WorldContextObject World context
	 * @param Effect Effect type to trigger
	 * @param PlayerIndex Player index for local effects (default 0)
	 */
	UFUNCTION(BlueprintCallable, Category = "Narrative|Short Stories", meta = (WorldContext = "WorldContextObject"))
	static void TriggerStoryEffect(UObject* WorldContextObject, EStoryEffect Effect, int32 PlayerIndex = 0);

	/**
	 * Get screen shake intensity for an effect type
	 * @param Effect Effect type
	 * @return Screen shake intensity (0.0 = none, 1.0 = high)
	 */
	UFUNCTION(BlueprintPure, Category = "Narrative|Short Stories")
	static float GetScreenShakeIntensity(EStoryEffect Effect);

	/**
	 * Split text into words (respecting punctuation)
	 * @param Text Text to split
	 * @return Array of words
	 */
	UFUNCTION(BlueprintPure, Category = "Narrative|Short Stories")
	static TArray<FString> SplitTextIntoWords(const FString& Text);

	/**
	 * Get recommended animation duration for an animation type
	 * @param AnimType Animation type
	 * @param WordCount Number of words in text
	 * @return Recommended duration in seconds
	 */
	UFUNCTION(BlueprintPure, Category = "Narrative|Short Stories")
	static float GetAnimationDuration(EStoryLineAnimation AnimType, int32 WordCount);

	/**
	 * Get recommended animation duration for a story line (convenience function)
	 * @param Line Story line to calculate duration for
	 * @return Recommended duration in seconds
	 */
	UFUNCTION(BlueprintPure, Category = "Narrative|Short Stories")
	static float GetAnimationDurationForLine(const FStoryLine& Line);

	/**
	 * Get pause duration in seconds for a story line (convenience function)
	 * @param Line Story line to get pause duration for
	 * @return Duration in seconds
	 */
	UFUNCTION(BlueprintPure, Category = "Narrative|Short Stories")
	static float GetPauseDurationSecondsForLine(const FStoryLine& Line);

	/**
	 * Calculate centered position for text
	 * @param CanvasSize Size of the canvas
	 * @param TextSize Size of the text block
	 * @return Centered position
	 */
	UFUNCTION(BlueprintPure, Category = "Narrative|Short Stories")
	static FVector2D CalculateCenteredPosition(FVector2D CanvasSize, FVector2D TextSize);

	/**
	 * Calculate line vertical position for multi-line layouts
	 * @param LineIndex Index of the line (0-based)
	 * @param TotalLines Total number of lines
	 * @param CanvasSize Size of the canvas
	 * @param LineHeight Height of a single line
	 * @return Vertical center position for the line
	 */
	UFUNCTION(BlueprintPure, Category = "Narrative|Short Stories")
	static float CalculateLineVerticalPosition(int32 LineIndex, int32 TotalLines, FVector2D CanvasSize, float LineHeight);
};
