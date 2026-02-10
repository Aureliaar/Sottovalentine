// Copyright Theory of Magic. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ShortStoryStructs.h"
#include "Containers/Ticker.h"
#include "ShortStorySubsystem.generated.h"

/**
 * Playback state enum for state machine
 */
UENUM(BlueprintType)
enum class EStoryPlaybackState : uint8
{
	Idle,
	PlayingLine,
	PausingAfterLine,
	TransitioningScreen,
	Completed
};

/**
 * Delegate for story completion events
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStoryCompleted);

/**
 * Delegate for screen change events
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnScreenChanged, int32, NewScreenIndex);

// Profiling stats
DECLARE_STATS_GROUP(TEXT("ShortStory"), STATGROUP_ShortStory, STATCAT_Advanced);
DECLARE_CYCLE_STAT_EXTERN(TEXT("LoadStory"), STAT_ShortStory_LoadStory, STATGROUP_ShortStory, SHORTSTORY_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Tick"), STAT_ShortStory_Tick, STATGROUP_ShortStory, SHORTSTORY_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("GetCurrentScreenState"), STAT_ShortStory_GetScreenState, STATGROUP_ShortStory, SHORTSTORY_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("StartLine"), STAT_ShortStory_StartLine, STATGROUP_ShortStory, SHORTSTORY_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("ParseStory"), STAT_ShortStory_ParseStory, STATGROUP_ShortStory, SHORTSTORY_API);

/**
 * Game instance subsystem for loading, caching, and playing short stories (.tos files)
 *
 * Stories are parsed from text files at runtime and cached for fast repeated access.
 * Files are located in {ProjectContentDir}/Stories/ directory.
 *
 * This subsystem also handles story playback state management. Blueprint widgets
 * poll GetCurrentScreenState() to retrieve the current screen with resolved text
 * based on animation progress.
 *
 * Example usage:
 *   UShortStorySubsystem* Subsystem = GetGameInstance()->GetSubsystem<UShortStorySubsystem>();
 *   Subsystem->StartStory("Orazio.tos");
 *   // In widget Tick: FStoryScreenState State = Subsystem->GetCurrentScreenState();
 */
UCLASS(config=Game)
class SHORTSTORY_API UShortStorySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ========================================
	// Event Dispatchers
	// ========================================

	/** Broadcast when a story completes playback */
	UPROPERTY(BlueprintAssignable, Category = "Narrative|Story Playback")
	FOnStoryCompleted OnStoryCompleted;

	/** Broadcast when advancing to a new screen */
	UPROPERTY(BlueprintAssignable, Category = "Narrative|Story Playback")
	FOnScreenChanged OnScreenChanged;
	
	/** Helper to load background texture from disk if needed */
	void ResolveBackgroundTexture(FStoryScreen& Screen, const FString& BaseSearchPath = TEXT(""));

	/**
	 * Load a story from .tos file
	 * @param StoryFileName Filename (e.g. "Orazio.tos")
	 * @param bForceReload If true, bypasses cache and re-parses the file
	 * @param bSuccess True if story was loaded successfully
	 * @return Parsed story data (check bSuccess for validity)
	 */
	UFUNCTION(BlueprintCallable, Category = "Narrative|Short Stories")
	FShortStory LoadStory(const FString& StoryFileName, bool bForceReload, bool& bSuccess);

	/**
	 * Get list of all available story files in Stories directory
	 * @return Array of story filenames (e.g. ["Orazio.tos", "AnotherStory.tos"])
	 */
	UFUNCTION(BlueprintCallable, Category = "Narrative|Short Stories")
	TArray<FString> GetAvailableStories();

	/**
	 * Check if a story is currently cached
	 * @param StoryFileName Filename to check
	 * @return True if story is in cache
	 */
	UFUNCTION(BlueprintPure, Category = "Narrative|Short Stories")
	bool IsStoryCached(const FString& StoryFileName) const;

	/**
	 * Clear a specific story from cache
	 * @param StoryFileName Filename to clear
	 */
	UFUNCTION(BlueprintCallable, Category = "Narrative|Short Stories")
	void ClearCachedStory(const FString& StoryFileName);

	/**
	 * Clear all cached stories
	 */
	UFUNCTION(BlueprintCallable, Category = "Narrative|Short Stories")
	void ClearAllCachedStories();

	// ========================================
	// Story Playback
	// ========================================

	/**
	 * Start playing a story
	 * @param StoryFileName Name of the .tos file (e.g., "Orazio.tos")
	 * @return True if story was loaded and playback started successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "Narrative|Story Playback")
	bool StartStory(const FString& StoryFileName);

	/**
	 * Stop current story playback
	 */
	UFUNCTION(BlueprintCallable, Category = "Narrative|Story Playback")
	void StopStory();

	/**
	 * Pause or resume playback
	 */
	UFUNCTION(BlueprintCallable, Category = "Narrative|Story Playback")
	void SetPaused(bool bPause);

	/**
	 * Check if a story is currently playing
	 */
	UFUNCTION(BlueprintPure, Category = "Narrative|Story Playback")
	bool IsPlaying() const { return bIsPlaying; }

	/**
	 * Check if playback is paused
	 */
	UFUNCTION(BlueprintPure, Category = "Narrative|Story Playback")
	bool IsPaused() const { return bIsPaused; }

	/**
	 * Get current screen index (0-based)
	 */
	UFUNCTION(BlueprintPure, Category = "Narrative|Story Playback")
	int32 GetCurrentScreenIndex() const { return CurrentScreenIndex; }

	/**
	 * Get current line index within current screen (0-based)
	 */
	UFUNCTION(BlueprintPure, Category = "Narrative|Story Playback")
	int32 GetCurrentLineIndex() const { return CurrentLineIndex; }

	/**
	 * Get current playback state
	 */
	UFUNCTION(BlueprintPure, Category = "Narrative|Story Playback")
	EStoryPlaybackState GetPlaybackState() const { return CurrentState; }

	/**
	 * Get the current line being played
	 */
	UFUNCTION(BlueprintPure, Category = "Narrative|Story Playback")
	FStoryLine GetCurrentLine() const;

	/**
	 * Get the current screen being displayed
	 */
	UFUNCTION(BlueprintPure, Category = "Narrative|Story Playback")
	FStoryScreen GetCurrentScreen() const;

	/**
	 * Get current screen state with resolved text based on animation progress
	 * Call this from Blueprint Tick to get current state for rendering
	 */
	UFUNCTION(BlueprintPure, Category = "Narrative|Story Playback")
	FStoryScreenState GetCurrentScreenState() const;

	/**
	 * Check if story is currently waiting for input (in Wait pause)
	 */
	UFUNCTION(BlueprintPure, Category = "Narrative|Story Playback")
	bool IsWaitingForInput() const;

	/**
	 * Advance story from a Wait pause if currently waiting for input
	 * This is the primary method for Blueprint widgets to call on player input
	 * @return True if advanced, false if not in a Wait pause
	 */
	UFUNCTION(BlueprintCallable, Category = "Narrative|Story Playback")
	bool ContinueStory();

	// ========================================
	// Timing Configuration
	// ========================================

	/**
	 * Get timing configuration for a speed type
	 * @param Speed Speed type (Standard, Fast, Slow)
	 * @return Timing configuration struct
	 */
	UFUNCTION(BlueprintPure, Category = "Narrative|Story Playback")
	FStoryAnimationTiming GetSpeedTiming(EStorySpeed Speed) const;

	/**
	 * Get pause duration for a pause type
	 * @param PauseType Pause type enum
	 * @return Duration in seconds
	 */
	UFUNCTION(BlueprintPure, Category = "Narrative|Story Playback")
	float GetPauseDuration(EStoryPauseDuration PauseType) const;

	/**
	 * Get screen transition pause duration
	 * @return Duration in seconds
	 */
	UFUNCTION(BlueprintPure, Category = "Narrative|Story Playback")
	float GetScreenTransitionPause() const { return ScreenTransitionPauseSeconds; }

	/**
	 * Get fade window (trail) duration
	 * @return Duration in seconds
	 */
	UFUNCTION(BlueprintPure, Category = "Narrative|Story Playback")
	float GetFadeWindow() const { return FadeWindowSeconds; }

	/**
	 * Go to a specific screen by index during playback
	 * Clears playback caches and transitions to the target screen
	 * @param ScreenIndex The screen index to navigate to (0-based)
	 * @return True if navigation succeeded, false if not playing or index out of range
	 */
	UFUNCTION(BlueprintCallable, Category = "Narrative|Story Playback")
	bool GoToScreen(int32 ScreenIndex);

	/**
	 * Go to a specific screen by name during playback
	 * Searches for a screen whose Name matches (case-insensitive)
	 * @param ScreenName The screen name to navigate to (e.g., "SCREEN_01_INTRO")
	 * @return True if navigation succeeded, false if not playing or name not found
	 */
	UFUNCTION(BlueprintCallable, Category = "Narrative|Story Playback")
	bool GoToScreenByName(const FString& ScreenName);

	/**
	 * Get the names of all screens in the current story
	 * @return Array of screen names (empty if no story is loaded)
	 */
	UFUNCTION(BlueprintPure, Category = "Narrative|Story Playback")
	TArray<FString> GetScreenNames() const;

	// ========================================
	// Debug Functions
	// ========================================

	/**
	 * [DEBUG] Skip to next screen immediately
	 */
	UFUNCTION(BlueprintCallable, Category = "Narrative|Story Playback|Debug")
	void DebugSkipToNextScreen();

	/**
	 * [DEBUG] Skip to previous screen
	 */
	UFUNCTION(BlueprintCallable, Category = "Narrative|Story Playback|Debug")
	void DebugSkipToPreviousScreen();

	/**
	 * [DEBUG] Jump to a specific screen index
	 * @param ScreenIndex The screen index to jump to (0-based)
	 */
	UFUNCTION(BlueprintCallable, Category = "Narrative|Story Playback|Debug")
	void DebugJumpToScreen(int32 ScreenIndex);

	/**
	 * [DEBUG] Skip current line and advance to next
	 */
	UFUNCTION(BlueprintCallable, Category = "Narrative|Story Playback|Debug")
	void DebugSkipCurrentLine();

	/**
	 * Calculate total duration for a line of text based on its animation
	 * @param Line The line to calculate duration for
	 * @return Total duration in seconds
	 */
	UFUNCTION(BlueprintPure, Category = "Narrative|Story Playback")
	float CalculateLineDuration(const FStoryLine& Line) const;

	/**
	 * Calculate total duration for a line of text based on speed (Typewriter only)
	 * @param Text The text to calculate duration for
	 * @param Speed Speed type
	 * @return Total duration in seconds
	 */
	UFUNCTION(BlueprintPure, Category = "Narrative|Story Playback")
	float CalculateTypewriterDuration(const FString& Text, EStorySpeed Speed) const;

	/**
	 * Map animation type to speed type
	 * @param AnimType Animation modifier type
	 * @return Corresponding speed type
	 */


	UFUNCTION(BlueprintPure, Category = "Narrative|Story Playback")
	static EStorySpeed GetSpeedForAnimation(EStoryLineAnimation AnimType);

	/**
	 * Calculate which character index corresponds to a given elapsed time
	 * @param Text The text to check
	 * @param Speed Speed type
	 * @param Time Elapsed time in seconds
	 * @return 0-based character index
	 */
	int32 GetCharacterIndexAtTime(const FString& Text, EStorySpeed Speed, float Time) const;

private:
	/**
	 * Get absolute file path for a story file
	 * @param StoryFileName Filename (e.g. "Orazio.tos")
	 * @return Full path to file
	 */
	FString GetStoryFilePath(const FString& StoryFileName) const;

	/**
	 * Get the Stories directory path
	 * @return Absolute path to Stories directory
	 */
	FString GetStoriesDirectory() const;

	/** Cache of parsed stories (key = filename, value = parsed story) */
	TMap<FString, FShortStory> CachedStories;

	/** Critical section for thread-safe cache access */
	mutable FCriticalSection CacheMutex;

	// ========================================
	// Timing Configuration Data
	// ========================================

	/** Loaded speed timings from CSV files */
	TMap<EStorySpeed, FStoryAnimationTiming> SpeedTimings;

	/** Loaded pause durations from CSV file */
	TMap<EStoryPauseDuration, float> PauseDurations;

	/** Screen transition pause duration (seconds) */
	float ScreenTransitionPauseSeconds = 1.5f;

	/** Fade window durations (seconds) for shader trail */
	float FadeWindowSeconds = 0.5f;

	/** Line break pause as percentage of fade window */
	float LineBreakPercent = 0.66f;

public:
	/** Maximum line length for auto-wrapping text (configurable based on resolution/UI scale) */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Display", meta = (ClampMin = "20", ClampMax = "300"))
	int32 MaxLineLength = 80;

private:

	/** Time elapsed during screen transition */
	float TransitionElapsedTime = 0.0f;

	/** Whether timing configs have been loaded */
	bool bTimingConfigsLoaded = false;

	/**
	 * Load timing configuration from CSV files
	 * Logs error if files are missing
	 */
	void LoadTimingConfigs();

	/**
	 * Get the Config directory path (under Stories)
	 * @return Absolute path to Config directory
	 */
	FString GetConfigDirectory() const;

	// ========================================
	// Playback State
	// ========================================

	/** Currently playing story */
	FShortStory CurrentStory;

	/** Current screen index (0-based) */
	int32 CurrentScreenIndex = 0;

	/** Current line index within current screen (0-based) */
	int32 CurrentLineIndex = 0;

	/** Time elapsed since current line started */
	float LineElapsedTime = 0.0f;

	/** Total duration for current line animation */
	float LineDuration = 0.0f;

	/** Time elapsed since pause started */
	float PauseElapsedTime = 0.0f;

	/** Total pause duration after line */
	float PauseDuration = 0.0f;

	/** Time elapsed since screen started (for timed events) */
	float ScreenElapsedTime = 0.0f;

	/** Start times for each line on the current screen (relative to ScreenElapsedTime) */
	TArray<float> LineStartTimes;

	/** Is a story currently playing? */
	bool bIsPlaying = false;

	/** Is playback paused? */
	bool bIsPaused = false;

	/** Is the current pause a "wait for input" pause? */
	bool bIsWaitingForInput = false;

	/** Current playback state */
	EStoryPlaybackState CurrentState = EStoryPlaybackState::Idle;

	/** Indices of timed events already processed on current screen */
	TSet<int32> ProcessedTimedEventIndices;

	/** Ticker handle for playback updates */
	FTSTicker::FDelegateHandle TickerHandle;

	// ========================================
	// Playback Methods
	// ========================================

	/**
	 * Reset playback caches for a screen jump (clears timers, event indices, line start times)
	 * @param TargetScreenIndex The screen index being jumped to (used to size LineStartTimes)
	 */
	void ResetScreenState(int32 TargetScreenIndex);

	/**
	 * Tick function called by FTSTicker
	 */
	bool Tick(float DeltaTime);

	/**
	 * Start playing a specific line
	 */
	void StartLine(int32 LineIndex);

	/**
	 * Start pause after line completes
	 */
	void StartPause();

	/**
	 * Advance to next line or screen
	 */
	void AdvanceToNextLineOrScreen();

	/**
	 * Advance to next screen
	 */
	void AdvanceToNextScreen();

	/**
	 * Process timed events based on screen elapsed time
	 */
	void ProcessTimedEvents();

	/**
	 * Called when screen transition completes
	 */
	void OnScreenTransitionComplete();


};
