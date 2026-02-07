// Copyright Theory of Magic. All Rights Reserved.

#include "ShortStory.h"
#include "ShortStorySubsystem.h"
#include "ShortStoryParser.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

DEFINE_LOG_CATEGORY(LogShortStory);

#define LOCTEXT_NAMESPACE "FShortStoryModule"

// ========================================
// Console Commands
// ========================================

static void ListStories(const TArray<FString>& Args, UWorld* World)
{
	if (!World || !World->GetGameInstance())
	{
		UE_LOG(LogShortStory, Error, TEXT("Story.List: No valid world or game instance"));
		return;
	}

	UShortStorySubsystem* Subsystem = World->GetGameInstance()->GetSubsystem<UShortStorySubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogShortStory, Error, TEXT("Story.List: ShortStorySubsystem not available"));
		return;
	}

	TArray<FString> Stories = Subsystem->GetAvailableStories();

	UE_LOG(LogShortStory, Display, TEXT("=== Available Stories (%d) ==="), Stories.Num());
	for (const FString& Story : Stories)
	{
		bool bCached = Subsystem->IsStoryCached(Story);
		UE_LOG(LogShortStory, Display, TEXT("  %s %s"), *Story, bCached ? TEXT("[CACHED]") : TEXT(""));
	}
}

static void LoadStoryCommand(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() < 1)
	{
		UE_LOG(LogShortStory, Error, TEXT("Story.Load: Missing filename argument. Usage: Story.Load <filename.tos>"));
		return;
	}

	if (!World || !World->GetGameInstance())
	{
		UE_LOG(LogShortStory, Error, TEXT("Story.Load: No valid world or game instance"));
		return;
	}

	UShortStorySubsystem* Subsystem = World->GetGameInstance()->GetSubsystem<UShortStorySubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogShortStory, Error, TEXT("Story.Load: ShortStorySubsystem not available"));
		return;
	}

	FString Filename = Args[0];
	bool bForceReload = Args.Contains(TEXT("-force"));

	UE_LOG(LogShortStory, Display, TEXT("=== Loading Story: %s ==="), *Filename);

	bool bSuccess = false;
	FShortStory Story = Subsystem->LoadStory(Filename, bForceReload, bSuccess);

	if (!bSuccess)
	{
		UE_LOG(LogShortStory, Error, TEXT("Failed to load story '%s' (check log for parse errors)"), *Filename);
		return;
	}

	// Print story details
	UE_LOG(LogShortStory, Display, TEXT("Title: %s"), *Story.Title);
	UE_LOG(LogShortStory, Display, TEXT("OST: %s"), *Story.OST.ToSoftObjectPath().ToString());
	UE_LOG(LogShortStory, Display, TEXT("Screens: %d"), Story.Screens.Num());
	UE_LOG(LogShortStory, Display, TEXT(""));

	// Print each screen summary
	for (int32 i = 0; i < Story.Screens.Num(); ++i)
	{
		const FStoryScreen& Screen = Story.Screens[i];
		UE_LOG(LogShortStory, Display, TEXT("  [SCREEN_%02d]"), i + 1);
		UE_LOG(LogShortStory, Display, TEXT("    Background: %s"), *Screen.Background.ToSoftObjectPath().ToString());
		UE_LOG(LogShortStory, Display, TEXT("    Transition: %s"), *UEnum::GetValueAsString(Screen.TransitionType));
		UE_LOG(LogShortStory, Display, TEXT("    Lines: %d"), Screen.Lines.Num());
		UE_LOG(LogShortStory, Display, TEXT("    Timed Events: %d"), Screen.TimedEvents.Num());

		// Print first few lines as preview
		int32 PreviewCount = FMath::Min(3, Screen.Lines.Num());
		for (int32 j = 0; j < PreviewCount; ++j)
		{
			const FStoryLine& Line = Screen.Lines[j];
			FString Preview = Line.Text.Left(60);
			if (Line.Text.Len() > 60)
			{
				Preview += TEXT("...");
			}
			UE_LOG(LogShortStory, Display, TEXT("      - \"%s\" [%s]"), *Preview, *UEnum::GetValueAsString(Line.AnimationType));
		}

		if (Screen.Lines.Num() > PreviewCount)
		{
			UE_LOG(LogShortStory, Display, TEXT("      ... and %d more lines"), Screen.Lines.Num() - PreviewCount);
		}

		UE_LOG(LogShortStory, Display, TEXT(""));
	}

	UE_LOG(LogShortStory, Display, TEXT("Story loaded successfully!"));
}

static void ClearCacheCommand(const TArray<FString>& Args, UWorld* World)
{
	if (!World || !World->GetGameInstance())
	{
		UE_LOG(LogShortStory, Error, TEXT("Story.ClearCache: No valid world or game instance"));
		return;
	}

	UShortStorySubsystem* Subsystem = World->GetGameInstance()->GetSubsystem<UShortStorySubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogShortStory, Error, TEXT("Story.ClearCache: ShortStorySubsystem not available"));
		return;
	}

	Subsystem->ClearAllCachedStories();
	UE_LOG(LogShortStory, Display, TEXT("Story cache cleared"));
}

static void DebugNextScreen(const TArray<FString>& Args, UWorld* World)
{
	if (!World || !World->GetGameInstance())
	{
		UE_LOG(LogShortStory, Error, TEXT("Story.NextScreen: No valid world or game instance"));
		return;
	}

	UShortStorySubsystem* Subsystem = World->GetGameInstance()->GetSubsystem<UShortStorySubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogShortStory, Error, TEXT("Story.NextScreen: ShortStorySubsystem not available"));
		return;
	}

	Subsystem->DebugSkipToNextScreen();
}

static void DebugPrevScreen(const TArray<FString>& Args, UWorld* World)
{
	if (!World || !World->GetGameInstance())
	{
		UE_LOG(LogShortStory, Error, TEXT("Story.PrevScreen: No valid world or game instance"));
		return;
	}

	UShortStorySubsystem* Subsystem = World->GetGameInstance()->GetSubsystem<UShortStorySubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogShortStory, Error, TEXT("Story.PrevScreen: ShortStorySubsystem not available"));
		return;
	}

	Subsystem->DebugSkipToPreviousScreen();
}

static void DebugJumpToScreen(const TArray<FString>& Args, UWorld* World)
{
	if (!World || !World->GetGameInstance())
	{
		UE_LOG(LogShortStory, Error, TEXT("Story.JumpToScreen: No valid world or game instance"));
		return;
	}

	if (Args.Num() < 1)
	{
		UE_LOG(LogShortStory, Error, TEXT("Story.JumpToScreen: Missing screen index. Usage: Story.JumpToScreen <index>"));
		return;
	}

	UShortStorySubsystem* Subsystem = World->GetGameInstance()->GetSubsystem<UShortStorySubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogShortStory, Error, TEXT("Story.JumpToScreen: ShortStorySubsystem not available"));
		return;
	}

	int32 ScreenIndex = FCString::Atoi(*Args[0]);
	Subsystem->DebugJumpToScreen(ScreenIndex);
}

static void DebugSkipLine(const TArray<FString>& Args, UWorld* World)
{
	if (!World || !World->GetGameInstance())
	{
		UE_LOG(LogShortStory, Error, TEXT("Story.SkipLine: No valid world or game instance"));
		return;
	}

	UShortStorySubsystem* Subsystem = World->GetGameInstance()->GetSubsystem<UShortStorySubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogShortStory, Error, TEXT("Story.SkipLine: ShortStorySubsystem not available"));
		return;
	}

	Subsystem->DebugSkipCurrentLine();
}

// Register console commands
static FAutoConsoleCommandWithWorldAndArgs GListStoriesCommand(
	TEXT("Story.List"),
	TEXT("List all available story files in Content/Stories/"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ListStories)
);

static FAutoConsoleCommandWithWorldAndArgs GLoadStoryCommand(
	TEXT("Story.Load"),
	TEXT("Load and parse a story file. Usage: Story.Load <filename.tos>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&LoadStoryCommand)
);

static FAutoConsoleCommandWithWorldAndArgs GClearCacheCommand(
	TEXT("Story.ClearCache"),
	TEXT("Clear all cached stories"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ClearCacheCommand)
);

static FAutoConsoleCommandWithWorldAndArgs GDebugNextScreenCommand(
	TEXT("Story.NextScreen"),
	TEXT("[DEBUG] Skip to next screen"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&DebugNextScreen)
);

static FAutoConsoleCommandWithWorldAndArgs GDebugPrevScreenCommand(
	TEXT("Story.PrevScreen"),
	TEXT("[DEBUG] Go to previous screen"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&DebugPrevScreen)
);

static FAutoConsoleCommandWithWorldAndArgs GDebugJumpToScreenCommand(
	TEXT("Story.JumpToScreen"),
	TEXT("[DEBUG] Jump to specific screen. Usage: Story.JumpToScreen <index>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&DebugJumpToScreen)
);

static FAutoConsoleCommandWithWorldAndArgs GDebugSkipLineCommand(
	TEXT("Story.SkipLine"),
	TEXT("[DEBUG] Skip current line"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&DebugSkipLine)
);

// ========================================
// Module Implementation
// ========================================

void FShortStoryModule::StartupModule()
{
	UE_LOG(LogShortStory, Log, TEXT("ShortStory module started"));
}

void FShortStoryModule::ShutdownModule()
{
	UE_LOG(LogShortStory, Log, TEXT("ShortStory module shutdown"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FShortStoryModule, ShortStory)
