// Copyright Theory of Magic. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture2D.h"
#include "ShortStoryStructs.generated.h"

/**
 * Animation modifier types for story text lines (positioning/visual style)
 * These are visual modifiers - actual timing is controlled by EStorySpeed
 * Matches the animation types from orazio_formatted.txt
 */
UENUM(BlueprintType)
enum class EStoryLineAnimation : uint8
{
	Typewriter			UMETA(DisplayName = "Typewriter"),
	LeftToRight			UMETA(DisplayName = "Left to Right"),
	Paragraph			UMETA(DisplayName = "Paragraph (All at Once)"),
	TopDown				UMETA(DisplayName = "Top Down"),
	WordRain			UMETA(DisplayName = "Word Rain"),
	Snake				UMETA(DisplayName = "Snake (Wavy)")
};

/**
 * Speed types for typewriter text reveal
 * Controls timing - loaded from Speed_*.csv files
 */
UENUM(BlueprintType)
enum class EStorySpeed : uint8
{
	Standard			UMETA(DisplayName = "Standard Speed"),
	Fast				UMETA(DisplayName = "Fast Speed"),
	Slow				UMETA(DisplayName = "Slow Speed")
};

/**
 * Timing configuration for typewriter text reveal
 * Loaded from Speed_*.csv files at runtime
 */
USTRUCT(BlueprintType)
struct FStoryAnimationTiming
{
	GENERATED_BODY()

	/** Seconds before each letter appears */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story")
	float PerLetter = 0.04f;

	/** Extra delay at word boundary (space) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story")
	float ExtraAtSpace = 0.08f;

	/** Extra delay at sentence-ending punctuation (. ! ?) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story")
	float ExtraAtPeriod = 0.3f;

	/** Extra delay at mid-sentence punctuation (, ;) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story")
	float ExtraAtComma = 0.2f;

	/** Duration for block-based animations (Paragraph, TopDown, WordRain) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story")
	float BlockDuration = 2.0f;


	/** Extra delay at colon punctuation (:) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story")
	float ExtraAtColon = 0.4f;

	FStoryAnimationTiming() = default;
};


/**
 * Pause duration after a line
 */
UENUM(BlueprintType)
enum class EStoryPauseDuration : uint8
{
	None				UMETA(DisplayName = "None (0s)"),
	Short				UMETA(DisplayName = "Short (0.5s)"),
	Standard			UMETA(DisplayName = "Standard (1.0s)"),
	Long				UMETA(DisplayName = "Long (2.0s)"),
	LineBreak			UMETA(DisplayName = "Implicit Line Break"),
	Wait				UMETA(DisplayName = "Wait for Input")
};

/**
 * Visual/audio effects for story lines
 */
UENUM(BlueprintType)
enum class EStoryEffect : uint8
{
	None				UMETA(DisplayName = "None"),
	ShakeLow			UMETA(DisplayName = "Screen Shake (Low)"),
	ShakeMed			UMETA(DisplayName = "Screen Shake (Medium)"),
	ShakeHigh			UMETA(DisplayName = "Screen Shake (High)"),
	Storm				UMETA(DisplayName = "Storm (Lightning + Thunder)")
};

/**
 * Screen transition types
 */
UENUM(BlueprintType)
enum class EStoryTransition : uint8
{
	Instant				UMETA(DisplayName = "Instant (No Transition)"),
	Fade				UMETA(DisplayName = "Fade"),
	Crossfade			UMETA(DisplayName = "Crossfade")
};

/**
 * Timed event types (SFX, VFX, etc.)
 */
UENUM(BlueprintType)
enum class EStoryTimedEventType : uint8
{
	SFX					UMETA(DisplayName = "Sound Effect"),
	VFX					UMETA(DisplayName = "Visual Effect"),
	Wait				UMETA(DisplayName = "Manual Wait"),
	BackgroundChange	UMETA(DisplayName = "Background Change")
};

/**
 * Timed event that triggers during a screen (SFX, VFX, waits)
 * Parsed from @ commands like @sfx, @vfx, @wait, @background
 */
USTRUCT(BlueprintType)
struct FStoryTimedEvent
{
	GENERATED_BODY()

	/** Type of timed event */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story")
	EStoryTimedEventType EventType = EStoryTimedEventType::SFX;

	/** Start time in seconds from screen start */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story")
	float StartTime = 0.0f;

	/** Duration in seconds (for VFX) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story")
	float Duration = 0.0f;

	/** Asset path/reference as string (FMOD path, Blueprint class, texture path) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story")
	FString AssetPath;

	FStoryTimedEvent() = default;
};

/**
 * Single line of story text with animation parameters
 * Corresponds to one pipe-delimited line in .tos file
 */
USTRUCT(BlueprintType)
struct FStoryLine
{
	GENERATED_BODY()

	/** The text content of this line */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story")
	FString Text;

	/** Animation type for this line */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story")
	EStoryLineAnimation AnimationType = EStoryLineAnimation::Typewriter;

	/** Animation speed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story")
	EStorySpeed Speed = EStorySpeed::Standard;

	/** Pause duration after this line */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story")
	EStoryPauseDuration PauseDuration = EStoryPauseDuration::None;

	/** Visual/audio effect for this line */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story")
	EStoryEffect Effect = EStoryEffect::None;

	/** Position offset in pixels (relative to auto-calculated position) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story")
	FVector2D PositionOffset = FVector2D::ZeroVector;

	FStoryLine() = default;
};

/**
 * Single screen/page of a short story
 * Corresponds to one [SCREEN_XX] section in .tos file
 */
USTRUCT(BlueprintType)
struct FStoryScreen
{
	GENERATED_BODY()

	/** Screen name from section header (e.g., "SCREEN_01_INTRO") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story")
	FString Name;

	/** Background texture for this screen (Asset) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story")
	TSoftObjectPtr<UTexture2D> Background;

	/** Background texture path (Raw file path for runtime loading) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story")
	FString BackgroundPath;

	/** Runtime loaded texture (Transient) */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Story")
	UTexture2D* RuntimeTexture = nullptr;

	/** Transition type when entering this screen */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story")
	EStoryTransition TransitionType = EStoryTransition::Fade;

	/** Array of text lines for this screen */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story")
	TArray<FStoryLine> Lines;

	/** Timed events (SFX, VFX) triggered during this screen */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story")
	TArray<FStoryTimedEvent> TimedEvents;

	FStoryScreen() = default;
};

/**
 * Complete short story with metadata and screens
 * Parsed from .tos file
 */
USTRUCT(BlueprintType)
struct FShortStory
{
	GENERATED_BODY()

	/** Title of the story */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story")
	FString Title;

	/** Audio event path for background music/OST (e.g., FMOD event path or audio file path) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story")
	FString OST;

	/** Array of screens/pages in this story */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story")
	TArray<FStoryScreen> Screens;

	/** Source filename (for debugging) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story")
	FString SourceFileName;

	FShortStory() = default;

	/** Check if story is valid (has title and at least one screen) */
	bool IsValid() const
	{
		return !Title.IsEmpty() && Screens.Num() > 0;
	}
};

/**
 * Runtime state for a story line with resolved text based on animation progress
 */
USTRUCT(BlueprintType)
struct FStoryLineState
{
	GENERATED_BODY()



	/** Full text of the line */
	UPROPERTY(BlueprintReadOnly, Category = "Story")
	FString FullText;

	/** Animation type for this line */
	UPROPERTY(BlueprintReadOnly, Category = "Story")
	EStoryLineAnimation AnimationType = EStoryLineAnimation::Typewriter;

	/** Visual/audio effect for this line */
	UPROPERTY(BlueprintReadOnly, Category = "Story")
	EStoryEffect Effect = EStoryEffect::None;

	/** Position offset in pixels */
	UPROPERTY(BlueprintReadOnly, Category = "Story")
	FVector2D PositionOffset = FVector2D::ZeroVector;

	/** Animation progress (0.0 = start, 1.0 = complete) */
	UPROPERTY(BlueprintReadOnly, Category = "Story")
	float AnimationProgress = 0.0f;

	/** Current text progress for shader (0.0 = start, 1.0 = end of string) */
	UPROPERTY(BlueprintReadOnly, Category = "Story")
	float CurrentTextProgress = 0.0f;

	/** Past text progress for shader trail (Current minus FadeWindow) */
	UPROPERTY(BlueprintReadOnly, Category = "Story")
	float PastTextProgress = 0.0f;

	/** Is this line fully visible? */
	UPROPERTY(BlueprintReadOnly, Category = "Story")
	bool bIsFullyVisible = false;

	/** Is this line currently animating? */
	UPROPERTY(BlueprintReadOnly, Category = "Story")
	bool bIsAnimating = false;

	FStoryLineState() = default;
};

/**
 * Runtime state for a story screen with resolved line text
 */
USTRUCT(BlueprintType)
struct FStoryScreenState
{
	GENERATED_BODY()

	/** Background texture for this screen (Asset) */
	UPROPERTY(BlueprintReadOnly, Category = "Story")
	TSoftObjectPtr<UTexture2D> Background;

	/** Already loaded background (Runtime or Cached) */
	UPROPERTY(BlueprintReadOnly, Category = "Story")
	UTexture2D* ReadyBackground = nullptr;

	/** Array of line states (with resolved text) */
	UPROPERTY(BlueprintReadOnly, Category = "Story")
	TArray<FStoryLineState> Lines;

	/** Current line index being animated */
	UPROPERTY(BlueprintReadOnly, Category = "Story")
	int32 CurrentLineIndex = 0;

	/** Screen index in story */
	UPROPERTY(BlueprintReadOnly, Category = "Story")
	int32 ScreenIndex = 0;

	/** Is story playback active? */
	UPROPERTY(BlueprintReadOnly, Category = "Story")
	bool bIsPlaying = false;

	/** Is story playback complete? */
	UPROPERTY(BlueprintReadOnly, Category = "Story")
	bool bIsComplete = false;

	FStoryScreenState() = default;
};
