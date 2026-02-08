#include "ShortStorySubsystem.h"
#include "ShortStoryParser.h"
#include "ShortStoryBlueprintLibrary.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "TextureResource.h"

void UShortStorySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Load timing configuration from CSV files
	LoadTimingConfigs();

	UE_LOG(LogShortStory, Log, TEXT("ShortStorySubsystem initialized"));
}

void UShortStorySubsystem::Deinitialize()
{
	// Stop playback if active
	StopStory();

	// Clear cache
	{
		FScopeLock Lock(&CacheMutex);
		
		// Clean up runtime textures
		for (auto& Elem : CachedStories)
		{
			for (const FStoryScreen& Screen : Elem.Value.Screens)
			{
				if (Screen.RuntimeTexture)
				{
					Screen.RuntimeTexture->RemoveFromRoot();
				}
			}
		}
		
		CachedStories.Empty();
	}

	Super::Deinitialize();
}

FShortStory UShortStorySubsystem::LoadStory(const FString& StoryFileName, bool bForceReload, bool& bSuccess)
{
	bSuccess = false;

	// Validate filename
	if (StoryFileName.IsEmpty())
	{
		UE_LOG(LogShortStory, Error, TEXT("LoadStory: Empty filename"));
		return FShortStory();
	}

	// Check cache first (unless force reload)
	if (!bForceReload)
	{
		FScopeLock Lock(&CacheMutex);
		if (CachedStories.Contains(StoryFileName))
		{
			UE_LOG(LogShortStory, Log, TEXT("LoadStory: Loading '%s' from cache"), *StoryFileName);
			bSuccess = true;
			return CachedStories[StoryFileName];
		}
	}

	// Get file path
	// Use full path so we handle subdirectories correctly
	FString FullPath = GetStoryFilePath(StoryFileName);
	// We want to cache using the input name (e.g. "Folder/Story.tos") or the full path?
	// GetAvailableStories returns relative paths like "Folder/Story.tos".
	// StartStory passes "Folder/Story.tos".
	// GetStoryFilePath("Folder/Story.tos") -> "Stories/Folder/Story.tos".
	// Let's assume StoryFileName is the unique key (relative or absolute).
	
	FString FullPathInCached = FullPath; // For texture resolution later

	// Parse story from file
	FShortStory Story;
	TArray<FString> Errors;
	bool bParsed = UShortStoryParser::ParseStoryFile(FullPath, Story, Errors, MaxLineLength);

	if (!bParsed)
	{
		UE_LOG(LogShortStory, Error, TEXT("LoadStory: Failed to parse '%s'"), *StoryFileName);
		for (const FString& Error : Errors)
		{
			UE_LOG(LogShortStory, Error, TEXT("  - %s"), *Error);
		}
		return FShortStory();
	}

	// Log warnings if any
	if (Errors.Num() > 0)
	{
		UE_LOG(LogShortStory, Warning, TEXT("LoadStory: Parsed '%s' with %d warnings"), *StoryFileName, Errors.Num());
	}
	else
	{
		UE_LOG(LogShortStory, Log, TEXT("LoadStory: Successfully parsed '%s' (%d screens)"), *StoryFileName, Story.Screens.Num());
	}

	// Resolve runtime textures
	FString StoryBaseDir = FPaths::GetPath(FullPathInCached);
	for (FStoryScreen& Screen : Story.Screens)
	{
		ResolveBackgroundTexture(Screen, StoryBaseDir);
	}

	// Cache the story
	{
		FScopeLock Lock(&CacheMutex);
		CachedStories.Add(StoryFileName, Story);
	}

	bSuccess = true;
	return Story;
}

void UShortStorySubsystem::ResolveBackgroundTexture(FStoryScreen& Screen, const FString& BaseSearchPath)
{
	// If we already have an asset pointer, we're good (unless we want to override?)
	// But let's check if we need to load from disk
	if (Screen.BackgroundPath.IsEmpty())
	{
		return;
	}

	// If it looks like an asset path and we have a SoftPtr, ensure it is loaded
	if ((Screen.BackgroundPath.StartsWith(TEXT("/Game")) || Screen.BackgroundPath.StartsWith(TEXT("/Engine"))) 
		&& !Screen.Background.IsNull())
	{
		// Force synchronous load so it's ready immediately
		if (UTexture2D* LoadedAsset = Screen.Background.LoadSynchronous())
		{
		}
		return;
	}

	// It's a raw file path. Try to load it.
	FString FullPath = Screen.BackgroundPath;
	if (FPaths::IsRelative(FullPath))
	{
		// Order of precedence:
		// 1. Relative to exact Story File location (BaseSearchPath) - if provided
		if (!BaseSearchPath.IsEmpty())
		{
			FString StoryRelative = FPaths::Combine(BaseSearchPath, FullPath);
			if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*StoryRelative))
			{
				FullPath = StoryRelative;
				goto LoadFile; // Found it
			}
		}

		// 2. Relative to Stories Root Directory
		FString StoriesRelative = FPaths::Combine(GetStoriesDirectory(), FullPath);
		if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*StoriesRelative))
		{
			FullPath = StoriesRelative;
			goto LoadFile;
		}

		// 3. Relative to Project Directory
		FString ProjectRelative = FPaths::Combine(FPaths::ProjectDir(), FullPath);
		if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*ProjectRelative))
		{
			FullPath = ProjectRelative;
		}
	}

LoadFile:
	if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*FullPath))
	{
		UE_LOG(LogShortStory, Warning, TEXT("ResolveBackgroundTexture: File not found: %s (Base: %s)"), *FullPath, *BaseSearchPath);
		return;
	}

	// Load raw data
	TArray<uint8> RawData;
	if (!FFileHelper::LoadFileToArray(RawData, *FullPath))
	{
		UE_LOG(LogShortStory, Error, TEXT("ResolveBackgroundTexture: Failed to load file: %s"), *FullPath);
		return;
	}

	// Use IImageWrapper to load texture (works in both editor and packaged builds)
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));

	// Auto-detect format
	EImageFormat ImageFormat = ImageWrapperModule.DetectImageFormat(RawData.GetData(), RawData.Num());
	if (ImageFormat == EImageFormat::Invalid)
	{
		UE_LOG(LogShortStory, Error, TEXT("ResolveBackgroundTexture: Unable to detect image format: %s"), *FullPath);
		return;
	}

	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(ImageFormat);
	if (!ImageWrapper.IsValid() || !ImageWrapper->SetCompressed(RawData.GetData(), RawData.Num()))
	{
		UE_LOG(LogShortStory, Error, TEXT("ResolveBackgroundTexture: Failed to decompress image: %s"), *FullPath);
		return;
	}

	// Get uncompressed RGBA data (not BGRA - UE5 expects RGBA for CreateTransient)
	TArray64<uint8> UncompressedRGBA;
	if (!ImageWrapper->GetRaw(ERGBFormat::RGBA, 8, UncompressedRGBA))
	{
		UE_LOG(LogShortStory, Error, TEXT("ResolveBackgroundTexture: Failed to get raw image data: %s"), *FullPath);
		return;
	}

	// Create texture from raw data
	const int32 Width = ImageWrapper->GetWidth();
	const int32 Height = ImageWrapper->GetHeight();

	// Create the texture - this will allocate the platform data
	Screen.RuntimeTexture = UTexture2D::CreateTransient(Width, Height, PF_R8G8B8A8);

	if (Screen.RuntimeTexture)
	{
		// Configure texture settings
		Screen.RuntimeTexture->SRGB = true;
		Screen.RuntimeTexture->Filter = TextureFilter::TF_Bilinear;

		// Get the platform data that was created
		FTexturePlatformData* PlatformData = Screen.RuntimeTexture->GetPlatformData();
		if (PlatformData && PlatformData->Mips.Num() > 0)
		{
			// Get the first mip level
			FTexture2DMipMap& Mip = PlatformData->Mips[0];

			// Lock the bulk data for writing
			void* TextureData = Mip.BulkData.Lock(LOCK_READ_WRITE);

			// Copy our image data to the texture
			const int64 DataSize = static_cast<int64>(Width) * Height * 4;
			FMemory::Memcpy(TextureData, UncompressedRGBA.GetData(), DataSize);

			// Unlock the bulk data
			Mip.BulkData.Unlock();

			// Update the texture resource on the GPU
			Screen.RuntimeTexture->UpdateResource();

			UE_LOG(LogShortStory, Log, TEXT("ResolveBackgroundTexture: Created %dx%d texture from %s"), Width, Height, *FullPath);
		}
		else
		{
			UE_LOG(LogShortStory, Error, TEXT("ResolveBackgroundTexture: No platform data or mips for texture: %s"), *FullPath);
			Screen.RuntimeTexture = nullptr;
		}
	}

	if (Screen.RuntimeTexture)
	{
		// Make sure it doesn't get garbage collected immediately if it's transient
		Screen.RuntimeTexture->AddToRoot(); 
		
		UE_LOG(LogShortStory, Log, TEXT("ResolveBackgroundTexture: Loaded runtime texture from %s"), *FullPath);
	}
	else
	{
		UE_LOG(LogShortStory, Error, TEXT("ResolveBackgroundTexture: Failed to decode image: %s"), *FullPath);
	}
}

TArray<FString> UShortStorySubsystem::GetAvailableStories()
{
	TArray<FString> StoryFiles;

	FString StoriesDir = GetStoriesDirectory();
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// Check if directory exists
	if (!PlatformFile.DirectoryExists(*StoriesDir))
	{
		UE_LOG(LogShortStory, Warning, TEXT("GetAvailableStories: Stories directory does not exist: %s"), *StoriesDir);
		return StoryFiles;
	}

	// Ensure trailing separator for MakePathRelativeTo
	if (!StoriesDir.EndsWith(TEXT("/")))
	{
		StoriesDir += TEXT("/");
	}

	// Recursive visitor
	class FGenericRecursiveVisitor : public IPlatformFile::FDirectoryVisitor
	{
	public:
		TArray<FString>& Files;
		FString RootDir;

		FGenericRecursiveVisitor(TArray<FString>& InFiles, const FString& InRootDir)
			: Files(InFiles), RootDir(InRootDir) {}

		virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
		{
			if (bIsDirectory)
			{
				return true; // Continue recursion
			}

			FString Filename = FPaths::GetCleanFilename(FilenameOrDirectory);
			if (Filename.EndsWith(TEXT(".tos")))
			{
				// We want path relative to StoriesDir
				FString FullPath = FilenameOrDirectory;
				FString RelativePath = FullPath;
				FPaths::MakePathRelativeTo(RelativePath, *RootDir);
				Files.Add(RelativePath);
			}
			return true;
		}
	};

	FGenericRecursiveVisitor Visitor(StoryFiles, StoriesDir);
	PlatformFile.IterateDirectoryRecursively(*StoriesDir, Visitor);

	UE_LOG(LogShortStory, Log, TEXT("GetAvailableStories: Found %d story files in %s"), StoryFiles.Num(), *StoriesDir);

	return StoryFiles;
}

bool UShortStorySubsystem::IsStoryCached(const FString& StoryFileName) const
{
	FScopeLock Lock(&CacheMutex);
	return CachedStories.Contains(StoryFileName);
}

void UShortStorySubsystem::ClearCachedStory(const FString& StoryFileName)
{
	FScopeLock Lock(&CacheMutex);
	if (CachedStories.Remove(StoryFileName) > 0)
	{
		UE_LOG(LogShortStory, Log, TEXT("ClearCachedStory: Cleared '%s' from cache"), *StoryFileName);
	}
}

void UShortStorySubsystem::ClearAllCachedStories()
{
	FScopeLock Lock(&CacheMutex);
	int32 Count = CachedStories.Num();
	CachedStories.Empty();
	UE_LOG(LogShortStory, Log, TEXT("ClearAllCachedStories: Cleared %d stories from cache"), Count);
}

FString UShortStorySubsystem::GetStoryFilePath(const FString& StoryFileName) const
{
	return FPaths::Combine(GetStoriesDirectory(), StoryFileName);
}

FString UShortStorySubsystem::GetStoriesDirectory() const
{
	// Try project content directory first
	FString ProjectStoriesDir = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Stories"));
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	if (PlatformFile.DirectoryExists(*ProjectStoriesDir))
	{
		UE_LOG(LogShortStory, Log, TEXT("Using project Stories directory: %s"), *ProjectStoriesDir);
		return ProjectStoriesDir;
	}

	// Fallback to plugin content directory
	IPluginManager& PluginManager = IPluginManager::Get();
	TSharedPtr<IPlugin> Plugin = PluginManager.FindPlugin(TEXT("ShortStory"));
	if (Plugin.IsValid())
	{
		FString PluginStoriesDir = FPaths::Combine(Plugin->GetContentDir(), TEXT("Stories"));
		UE_LOG(LogShortStory, Log, TEXT("Using plugin Stories directory: %s"), *PluginStoriesDir);
		return PluginStoriesDir;
	}

	// Last resort: return project directory even if it doesn't exist yet
	UE_LOG(LogShortStory, Warning, TEXT("Stories directory not found, defaulting to project content directory"));
	return ProjectStoriesDir;
}

FString UShortStorySubsystem::GetConfigDirectory() const
{
	return FPaths::Combine(GetStoriesDirectory(), TEXT("Config"));
}

// ========================================
// Timing Configuration Implementation
// ========================================

void UShortStorySubsystem::LoadTimingConfigs()
{
	if (bTimingConfigsLoaded)
	{
		return;
	}

	FString ConfigDir = GetConfigDirectory();
	
	// Helper lambda to show error
	auto ShowError = [](const FString& Message)
	{
		UE_LOG(LogShortStory, Error, TEXT("SHORT STORY CONFIG ERROR: %s"), *Message);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 30.f, FColor::Red, 
				FString::Printf(TEXT("SHORT STORY ERROR: %s"), *Message));
		}
	};

	// Load global config (pauses, transitions)
	{
		FString GlobalPath = FPaths::Combine(ConfigDir, TEXT("ShortStoryGlobal.csv"));
		TArray<FString> Lines;
		if (!FFileHelper::LoadFileToStringArray(Lines, *GlobalPath))
		{
			ShowError(FString::Printf(TEXT("Missing required config file: %s"), *GlobalPath));
			return;
		}

		for (const FString& Line : Lines)
		{
			// Skip header and empty lines
			if (Line.IsEmpty() || Line.StartsWith(TEXT("---")))
			{
				continue;
			}

			TArray<FString> Parts;
			Line.ParseIntoArray(Parts, TEXT(","));
			if (Parts.Num() < 3)
			{
				continue;
			}

			FString Category = Parts[0].TrimStartAndEnd();
			FString Name = Parts[1].TrimStartAndEnd();
			float Value = FCString::Atof(*Parts[2].TrimStartAndEnd());

			if (Category == TEXT("Pause"))
			{
				if (Name == TEXT("None")) PauseDurations.Add(EStoryPauseDuration::None, Value);
				else if (Name == TEXT("Short")) PauseDurations.Add(EStoryPauseDuration::Short, Value);
				else if (Name == TEXT("Standard")) PauseDurations.Add(EStoryPauseDuration::Standard, Value);
				else if (Name == TEXT("Long")) PauseDurations.Add(EStoryPauseDuration::Long, Value);
			}
			else if (Category == TEXT("Transition"))
			{
				if (Name == TEXT("ScreenPause"))
				{
					ScreenTransitionPauseSeconds = Value;
				}
				else if (Name == TEXT("FadeWindow"))
				{
					FadeWindowSeconds = Value;
				}
				else if (Name == TEXT("LineBreakPercent"))
				{
					LineBreakPercent = Value;
				}
			}
		}

		UE_LOG(LogShortStory, Log, TEXT("Loaded ShortStoryGlobal.csv: %d pause durations, screen pause = %.2fs, fade window = %.2fs"), 
			PauseDurations.Num(), ScreenTransitionPauseSeconds, FadeWindowSeconds);
	}

	// Helper lambda to load speed config
	auto LoadSpeedConfig = [&](EStorySpeed Speed, const FString& FileName)
	{
		FString FilePath = FPaths::Combine(ConfigDir, FileName);
		TArray<FString> Lines;
		if (!FFileHelper::LoadFileToStringArray(Lines, *FilePath))
		{
			ShowError(FString::Printf(TEXT("Missing required config file: %s"), *FilePath));
			return false;
		}

		FStoryAnimationTiming Timing;
		for (const FString& Line : Lines)
		{
			if (Line.IsEmpty() || Line.StartsWith(TEXT("---")))
			{
				continue;
			}

			TArray<FString> Parts;
			Line.ParseIntoArray(Parts, TEXT(","));
			if (Parts.Num() < 3)
			{
				continue;
			}

			FString Name = Parts[1].TrimStartAndEnd();
			float Value = FCString::Atof(*Parts[2].TrimStartAndEnd());

			if (Name == TEXT("PerLetter")) Timing.PerLetter = Value;
			else if (Name == TEXT("ExtraAtSpace")) Timing.ExtraAtSpace = Value;
			else if (Name == TEXT("ExtraAtPeriod")) Timing.ExtraAtPeriod = Value;
			else if (Name == TEXT("ExtraAtComma")) Timing.ExtraAtComma = Value;
			else if (Name == TEXT("ExtraAtColon")) Timing.ExtraAtColon = Value;
			else if (Name == TEXT("BlockDuration")) Timing.BlockDuration = Value;
		}

		SpeedTimings.Add(Speed, Timing);
		UE_LOG(LogShortStory, Log, TEXT("Loaded %s: PerLetter=%.3f, ExtraAtSpace=%.3f, ExtraAtPeriod=%.3f, ExtraAtComma=%.3f, ExtraAtColon=%.3f"),
			*FileName, Timing.PerLetter, Timing.ExtraAtSpace, Timing.ExtraAtPeriod, Timing.ExtraAtComma, Timing.ExtraAtColon);
		return true;
	};

	// Load all speed configs
	LoadSpeedConfig(EStorySpeed::Standard, TEXT("Speed_Standard.csv"));
	LoadSpeedConfig(EStorySpeed::Fast, TEXT("Speed_Fast.csv"));
	LoadSpeedConfig(EStorySpeed::Slow, TEXT("Speed_Slow.csv"));

	bTimingConfigsLoaded = true;
	UE_LOG(LogShortStory, Log, TEXT("Timing configuration loaded: %d speeds, %d pauses"), 
		SpeedTimings.Num(), PauseDurations.Num());
}

FStoryAnimationTiming UShortStorySubsystem::GetSpeedTiming(EStorySpeed Speed) const
{
	if (const FStoryAnimationTiming* Found = SpeedTimings.Find(Speed))
	{
		return *Found;
	}
	// Return Standard if not found (shouldn't happen if configs loaded)
	if (const FStoryAnimationTiming* Standard = SpeedTimings.Find(EStorySpeed::Standard))
	{
		return *Standard;
	}
	return FStoryAnimationTiming();
}

float UShortStorySubsystem::GetPauseDuration(EStoryPauseDuration PauseType) const
{
	// Both LineBreak and None use the configurable LineBreakPercent
	if (PauseType == EStoryPauseDuration::LineBreak || PauseType == EStoryPauseDuration::None)
	{
		return FadeWindowSeconds * LineBreakPercent;
	}

	if (const float* Found = PauseDurations.Find(PauseType))
	{
		return *Found;
	}
	return 0.0f;
}

float UShortStorySubsystem::CalculateTypewriterDuration(const FString& Text, EStorySpeed Speed) const
{
	FStoryAnimationTiming Timing = GetSpeedTiming(Speed);
	float Total = 0.0f;

	for (TCHAR Char : Text)
	{
		Total += Timing.PerLetter;

		if (FChar::IsWhitespace(Char))
		{
			Total += Timing.ExtraAtSpace;
		}
		else if (Char == TEXT('.') || Char == TEXT('!') || Char == TEXT('?'))
		{
			Total += Timing.ExtraAtPeriod;
		}
		else if (Char == TEXT(','))
		{
			Total += Timing.ExtraAtComma;
		}
		else if (Char == TEXT(';') || Char == TEXT(':'))
		{
			// Semicolon treated as colon for now? Or comma?
			// Previous logic had : and ; with comma. 
			// Let's keep ; with comma or move to colon?
			// Users request was specifically about Colon. 
			// Original code: else if (Char == TEXT(',') || Char == TEXT(';') || Char == TEXT(':')) { ... ExtraAtComma }
			// New logic: Colon has own timing.
			if (Char == TEXT(':'))
			{
				Total += Timing.ExtraAtColon;
			}
			else
			{
				Total += Timing.ExtraAtComma;
			}
		}
	}

	return Total;
}

float UShortStorySubsystem::CalculateLineDuration(const FStoryLine& Line) const
{
	// Block animations (Paragraph, TopDown, WordRain, Snake, LeftToRight)
	// These use the fixed 'BlockDuration' defined in Speed config.
	if (Line.AnimationType != EStoryLineAnimation::Typewriter)
	{
		FStoryAnimationTiming Timing = GetSpeedTiming(Line.Speed);
		return Timing.BlockDuration;
	}

	// Typewriter behavior uses per-character timing
	return CalculateTypewriterDuration(Line.Text, Line.Speed);
}

EStorySpeed UShortStorySubsystem::GetSpeedForAnimation(EStoryLineAnimation AnimType)
{
	switch (AnimType)
	{
		// We could potentially map Typewriter + metadata back to speed, 
		// but since we removed Slow/Fast from enum, we assume Standard speed for Typewriter by default
		// unless we add speed override property. 
		// For now, Typewriter = Standard.
		case EStoryLineAnimation::Typewriter:
		default:
			return EStorySpeed::Standard;
	}
}

int32 UShortStorySubsystem::GetCharacterIndexAtTime(const FString& Text, EStorySpeed Speed, float Time) const
{
	if (Time <= 0.0f)
	{
		return 0;
	}

	FStoryAnimationTiming Timing = GetSpeedTiming(Speed);
	float CurrentTime = 0.0f;
	int32 TotalChars = Text.Len();

	for (int32 i = 0; i < TotalChars; ++i)
	{
		TCHAR Char = Text[i];
		float TypeTime = Timing.PerLetter;
		float ExtraTime = 0.0f;

		if (FChar::IsWhitespace(Char))
		{
			ExtraTime = Timing.ExtraAtSpace;
		}
		else if (Char == TEXT('.') || Char == TEXT('!') || Char == TEXT('?'))
		{
			ExtraTime = Timing.ExtraAtPeriod;
		}
		else if (Char == TEXT(','))
		{
			ExtraTime = Timing.ExtraAtComma;
		}
		else if (Char == TEXT(';') || Char == TEXT(':'))
		{
			if (Char == TEXT(':'))
			{
				ExtraTime = Timing.ExtraAtColon;
			}
			else
			{
				ExtraTime = Timing.ExtraAtComma;
			}
		}

		// Phase 1: Typing the character (Hidden)
		if (Time < CurrentTime + TypeTime)
		{
			// Still "typing" this character, it hasn't appeared yet
			return i;
		}
		CurrentTime += TypeTime;

		// Phase 2: Extra pause after character (Visible)
		if (Time < CurrentTime + ExtraTime)
		{
			// Character is typed and we are waiting in the "heavy pause"
			// So this character (i) should be visible
			return i + 1;
		}
		CurrentTime += ExtraTime;
	}

	// Time exceeds total duration, return end of string + 1? Or just length?
	// Progress calculation uses TotalLength, so returning Len is appropriate for "100%"
	return TotalChars;
}

// ========================================
// Story Playback Implementation
// ========================================

bool UShortStorySubsystem::StartStory(const FString& StoryFileName)
{
	// Load the story
	bool bSuccess = false;
	FShortStory LoadedStory = LoadStory(StoryFileName, false, bSuccess);
	if (!bSuccess)
	{
		UE_LOG(LogShortStory, Error, TEXT("StartStory: Failed to load story '%s'"), *StoryFileName);
		return false;
	}

	// Validate story has screens and lines
	if (LoadedStory.Screens.Num() == 0)
	{
		UE_LOG(LogShortStory, Error, TEXT("StartStory: Story '%s' has no screens"), *StoryFileName);
		return false;
	}

	if (LoadedStory.Screens[0].Lines.Num() == 0)
	{
		UE_LOG(LogShortStory, Error, TEXT("StartStory: Story '%s' first screen has no lines"), *StoryFileName);
		return false;
	}

	// Initialize playback state
	CurrentStory = LoadedStory;
	CurrentScreenIndex = 0;
	CurrentLineIndex = 0;
	ScreenElapsedTime = 0.0f;
	ProcessedTimedEventIndices.Empty();
	LineStartTimes.Init(-1.0f, CurrentStory.Screens[0].Lines.Num());
	bIsPlaying = true;
	bIsPaused = false;
	CurrentState = EStoryPlaybackState::PlayingLine;

	// Register ticker if not already registered
	if (!TickerHandle.IsValid())
	{
		TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateUObject(this, &UShortStorySubsystem::Tick)
		);
	}

	// Start playing first line
	StartLine(0);

	UE_LOG(LogShortStory, Log, TEXT("StartStory: Started story '%s' with %d screens"),
		*StoryFileName, CurrentStory.Screens.Num());

	return true;
}

void UShortStorySubsystem::StopStory()
{
	if (!bIsPlaying)
	{
		return;
	}

	bIsPlaying = false;
	bIsPaused = false;
	CurrentState = EStoryPlaybackState::Idle;
	CurrentScreenIndex = 0;
	CurrentLineIndex = 0;
	ScreenElapsedTime = 0.0f;
	LineElapsedTime = 0.0f;
	PauseElapsedTime = 0.0f;
	ProcessedTimedEventIndices.Empty();
	LineStartTimes.Empty();

	// Unregister ticker
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}

	UE_LOG(LogShortStory, Log, TEXT("StopStory: Story stopped"));
}

void UShortStorySubsystem::SetPaused(bool bPause)
{
	if (!bIsPlaying)
	{
		return;
	}

	bIsPaused = bPause;
	UE_LOG(LogShortStory, Log, TEXT("SetPaused: Playback %s"), bPause ? TEXT("paused") : TEXT("resumed"));
}

FStoryLine UShortStorySubsystem::GetCurrentLine() const
{
	if (!bIsPlaying || CurrentScreenIndex >= CurrentStory.Screens.Num())
	{
		return FStoryLine();
	}

	const FStoryScreen& CurrentScreen = CurrentStory.Screens[CurrentScreenIndex];
	if (CurrentLineIndex >= CurrentScreen.Lines.Num())
	{
		return FStoryLine();
	}

	return CurrentScreen.Lines[CurrentLineIndex];
}

FStoryScreen UShortStorySubsystem::GetCurrentScreen() const
{
	if (!bIsPlaying || CurrentScreenIndex >= CurrentStory.Screens.Num())
	{
		return FStoryScreen();
	}

	return CurrentStory.Screens[CurrentScreenIndex];
}

FStoryScreenState UShortStorySubsystem::GetCurrentScreenState() const
{
	FStoryScreenState State;

	// Set basic state info
	State.bIsPlaying = bIsPlaying;
	State.bIsComplete = (CurrentState == EStoryPlaybackState::Completed);
	State.ScreenIndex = CurrentScreenIndex;
	State.CurrentLineIndex = CurrentLineIndex;

	// If not playing or invalid state, return empty
	if (!bIsPlaying || CurrentScreenIndex >= CurrentStory.Screens.Num())
	{
		return State;
	}

	const FStoryScreen& CurrentScreen = CurrentStory.Screens[CurrentScreenIndex];
	
	// Prioritize runtime texture if available
	if (CurrentScreen.RuntimeTexture)
	{
		State.ReadyBackground = CurrentScreen.RuntimeTexture;
		// Keep State.Background empty/null to avoid AsyncLoad confusion? 
		// Or set it to null explicitly if it was implicitly null.
		// If we set State.Background = RuntimeTexture, AsyncLoad triggers warning on transient path.
		// So we leave State.Background as null (default from CurrentScreen.Background which is null).
	}
	else
	{
		State.Background = CurrentScreen.Background;
		// Check if asset is already loaded in memory
		if (UTexture2D* Loaded = CurrentScreen.Background.Get())
		{
			State.ReadyBackground = Loaded;
		}
	}

	// Build line states
	for (int32 i = 0; i < CurrentScreen.Lines.Num(); ++i)
	{
		const FStoryLine& SourceLine = CurrentScreen.Lines[i];
		FStoryLineState LineState;

		LineState.FullText = SourceLine.Text;
		LineState.AnimationType = SourceLine.AnimationType;
		LineState.Effect = SourceLine.Effect;
		LineState.PositionOffset = SourceLine.PositionOffset;

		// Calculate local time for this line if it has started
		float LineStartTime = -1.0f;
		if (LineStartTimes.IsValidIndex(i))
		{
			LineStartTime = LineStartTimes[i];
		}

		// If line has started (time >= 0), calculate progress
		if (LineStartTime >= 0.0f)
		{
			float LocalLineTime = ScreenElapsedTime - LineStartTime;
			// We can't easily access the original duration here without recalculating or storing it.
			// Re-calculating duration for every frame/line might be slightly expensive but acceptable.
			// Alternatively, since we used LineDuration member for the *Current* line, we might rely on that?
			// But that changes as we advance. 
			// Let's recalculate helper.
			// Calculate duration using new helper
			float ThisLineDuration = CalculateLineDuration(SourceLine);
			
			float Progress = (ThisLineDuration > 0.0f) ? (LocalLineTime / ThisLineDuration) : 1.0f;
			Progress = FMath::Clamp(Progress, 0.0f, 1.0f);

			LineState.AnimationProgress = Progress;
			LineState.bIsAnimating = (Progress < 1.0f); // Roughly accurate
			LineState.bIsFullyVisible = (Progress >= 1.0f);



			if (SourceLine.Text.IsEmpty())
			{
				LineState.CurrentTextProgress = 1.0f;
				LineState.PastTextProgress = 1.0f;
			}
			else
			{
				// Smooth interpolation: progress is based on time elapsed vs total duration
				// This treats pauses as "character weight" rather than discrete stops
				LineState.CurrentTextProgress = FMath::Clamp(LocalLineTime / ThisLineDuration, 0.0f, 1.0f);
				
				// Past progress uses absolute time (LocalLineTime - FadeWindow) / Duration
				// This allows PastProgress to "catch up" to 1.0 after the line finishes
				float PastTime = LocalLineTime - FadeWindowSeconds;
				LineState.PastTextProgress = FMath::Clamp(PastTime / ThisLineDuration, 0.0f, 1.0f);
			}
		}
		else
		{
			// Not started yet
			LineState.AnimationProgress = 0.0f;
			LineState.CurrentTextProgress = 0.0f;
			LineState.PastTextProgress = 0.0f;
			LineState.bIsFullyVisible = false;
			LineState.bIsAnimating = false;
		}

		State.Lines.Add(LineState);
	}

	return State;
}



bool UShortStorySubsystem::Tick(float DeltaTime)
{
	if (!bIsPlaying || bIsPaused)
	{
		return true; // Keep ticking
	}

	// Increment screen elapsed time (for timed events)
	ScreenElapsedTime += DeltaTime;

	// Check and fire timed events
	ProcessTimedEvents();

	// State machine logic
	switch (CurrentState)
	{
		case EStoryPlaybackState::PlayingLine:
		{
			LineElapsedTime += DeltaTime;
			if (LineElapsedTime >= LineDuration)
			{
				// Line animation complete
				StartPause();
			}
			break;
		}

		case EStoryPlaybackState::PausingAfterLine:
		{
			PauseElapsedTime += DeltaTime;
			if (PauseElapsedTime >= PauseDuration)
			{
				// Pause complete, advance to next line or screen
				AdvanceToNextLineOrScreen();
			}
			break;
		}

		case EStoryPlaybackState::TransitioningScreen:
		{
			// Wait for screen transition pause
			TransitionElapsedTime += DeltaTime;
			if (TransitionElapsedTime >= ScreenTransitionPauseSeconds)
			{
				OnScreenTransitionComplete();
			}
			break;
		}

		case EStoryPlaybackState::Completed:
		case EStoryPlaybackState::Idle:
		default:
			// No action needed
			break;
	}

	return true; // Keep ticking
}

void UShortStorySubsystem::StartLine(int32 LineIndex)
{
	if (CurrentScreenIndex >= CurrentStory.Screens.Num())
	{
		UE_LOG(LogShortStory, Error, TEXT("StartLine: Invalid screen index %d"), CurrentScreenIndex);
		return;
	}

	const FStoryScreen& CurrentScreen = CurrentStory.Screens[CurrentScreenIndex];
	if (LineIndex >= CurrentScreen.Lines.Num())
	{
		UE_LOG(LogShortStory, Error, TEXT("StartLine: Invalid line index %d on screen %d"), LineIndex, CurrentScreenIndex);
		return;
	}

	const FStoryLine& Line = CurrentScreen.Lines[LineIndex];

	// BLOCK ANIMATION LOGIC
	// Check if this line starts a block (TopDown or Paragraph)
	bool bIsBlockAnimation = (Line.AnimationType == EStoryLineAnimation::Paragraph || Line.AnimationType == EStoryLineAnimation::TopDown);

	if (bIsBlockAnimation)
	{
		// Find all lines in this block
		// A block consists of consecutive lines with the same animation type
		// and implicitly linked (e.g. by LineBreak pause, though usually the last one has the real pause)
		// Actually, let's just group consecutive lines of the SAME animation type 
		// until we hit a different type or the end of the screen.
		// NOTE: The parser splits wrapped text into lines with Pause=LineBreak, 
		// and the final line has the actual pause.
		
		int32 BlockEndIndex = LineIndex;
		for (int32 i = LineIndex + 1; i < CurrentScreen.Lines.Num(); ++i)
		{
			const FStoryLine& NextLine = CurrentScreen.Lines[i];
			if (NextLine.AnimationType == Line.AnimationType)
			{
				BlockEndIndex = i;
				// If this line has a significant pause (not LineBreak/None), it ends the logical block?
				// Or do we treat the whole sequence as one block regardless of pauses?
				// Usually "Paragraph" means "Everything until the user clicks" or "Everything in this chunk".
				// Given the parser splits one logical line into multiple FStoryLines, we should treat them as one block.
				// However, if the user explicitly wrote two Paragraph lines separated by a pipe, they might want them separate.
				// But Parser currently unifies them if they are in the same split group.
				// Let's assume contiguous lines of same type = one block.
				
				// Optional: Break if we hit a Hard Pause? 
				// But let's stick to contiguous same-type for now as a robust heuristic.
			}
			else
			{
				break;
			}
		}

		// Calculate timing for the block
		float CurrentDelay = 0.0f;
		float MaxFinishTime = 0.0f;
		float CascadeDelay = 0.2f; // Time between lines for TopDown

		for (int32 i = LineIndex; i <= BlockEndIndex; ++i)
		{
			// Set start time
			if (LineStartTimes.IsValidIndex(i))
			{
				LineStartTimes[i] = ScreenElapsedTime + CurrentDelay;
			}
			
			// Update delay for next line
			if (Line.AnimationType == EStoryLineAnimation::TopDown)
			{
				CurrentDelay += CascadeDelay;
			}
			else // Paragraph / FadeIn
			{
				// Parallel: No added delay
			}

			// Track when this line finishes
			float ThisDuration = CalculateLineDuration(CurrentScreen.Lines[i]);
			float FinishTime = CurrentDelay + ThisDuration; // Relative to start of block
			if (FinishTime > MaxFinishTime)
			{
				MaxFinishTime = FinishTime;
			}
		}

		// Set LineDuration to the total time until the *last* animation finishes
		// This ensures the state machine waits appropriately.
		// Note: MaxFinishTime includes the cascade delays.
		// However, for TopDown, we added delay *before* setting FinishTime.
		// Let's re-verify:
		// Line 0: Start=0, Dur=F. Finish=F. NextDelay=0.2.
		// Line 1: Start=0.2, Dur=F. Finish=0.2+F. NextDelay=0.4.
		// MaxFinishTime will be (BlockSize-1)*Delay + FadeWindow.
		LineDuration = MaxFinishTime;

		UE_LOG(LogShortStory, Verbose, TEXT("StartLine: Started BLOCK %s (%d lines) duration: %.2fs"), 
			*UEnum::GetValueAsString(Line.AnimationType), (BlockEndIndex - LineIndex + 1), LineDuration);

		// Advance CurrentLineIndex to end of block
		// calls to GetCurrentLine() will return the last line, which has the correct PauseDuration
		CurrentLineIndex = BlockEndIndex;
	}
	else
	{
		// Standard Single Line Behavior
		LineDuration = CalculateTypewriterDuration(Line.Text, Line.Speed);

		// Record start time
		if (LineStartTimes.IsValidIndex(LineIndex))
		{
			LineStartTimes[LineIndex] = ScreenElapsedTime;
		}

		UE_LOG(LogShortStory, Verbose, TEXT("StartLine: Started line %d/%d on screen %d (duration: %.2fs): %s"),
			LineIndex, CurrentScreen.Lines.Num() - 1, CurrentScreenIndex, LineDuration, *Line.Text);
	}

	// Reset line timer
	LineElapsedTime = 0.0f;
	CurrentState = EStoryPlaybackState::PlayingLine;
}

void UShortStorySubsystem::StartPause()
{
	const FStoryLine CurrentLine = GetCurrentLine();

	// Get pause duration from loaded CSV config
	PauseDuration = GetPauseDuration(CurrentLine.PauseDuration);

	// Reset pause timer
	PauseElapsedTime = 0.0f;
	CurrentState = EStoryPlaybackState::PausingAfterLine;

	UE_LOG(LogShortStory, Verbose, TEXT("StartPause: Starting pause of %.2fs after line %d"),
		PauseDuration, CurrentLineIndex);
}

void UShortStorySubsystem::AdvanceToNextLineOrScreen()
{
	if (CurrentScreenIndex >= CurrentStory.Screens.Num())
	{
		return;
	}

	const FStoryScreen& CurrentScreen = CurrentStory.Screens[CurrentScreenIndex];

	// Increment line index
	CurrentLineIndex++;

	// Check if more lines in current screen
	if (CurrentLineIndex < CurrentScreen.Lines.Num())
	{
		// More lines in screen, start next line
		StartLine(CurrentLineIndex);
	}
	else
	{
		// Screen complete, advance to next screen
		AdvanceToNextScreen();
	}
}

void UShortStorySubsystem::AdvanceToNextScreen()
{
	// Increment screen index
	CurrentScreenIndex++;

	// Check if more screens
	if (CurrentScreenIndex < CurrentStory.Screens.Num())
	{
		// More screens, start next screen
		const FStoryScreen& NextScreen = CurrentStory.Screens[CurrentScreenIndex];

		// Reset screen state
		CurrentLineIndex = 0;
		ScreenElapsedTime = 0.0f;
		ScreenElapsedTime = 0.0f;
		ProcessedTimedEventIndices.Empty();
		LineStartTimes.Init(-1.0f, NextScreen.Lines.Num());

		// Set state to transitioning
		CurrentState = EStoryPlaybackState::TransitioningScreen;

		UE_LOG(LogShortStory, Log, TEXT("AdvanceToNextScreen: Advanced to screen %d/%d"),
			CurrentScreenIndex, CurrentStory.Screens.Num() - 1);

		// For now, immediately complete transition and start first line
		// In the future, Blueprint could signal when transition is complete
		// OnScreenTransitionComplete() will be called next tick
	}
	else
	{
		// Story complete
		bIsPlaying = false;
		CurrentState = EStoryPlaybackState::Completed;

		// Unregister ticker
		if (TickerHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
			TickerHandle.Reset();
		}

		UE_LOG(LogShortStory, Log, TEXT("AdvanceToNextScreen: Story completed"));
	}
}

void UShortStorySubsystem::ProcessTimedEvents()
{
	if (CurrentScreenIndex >= CurrentStory.Screens.Num())
	{
		return;
	}

	const FStoryScreen& CurrentScreen = CurrentStory.Screens[CurrentScreenIndex];

	// Check each timed event
	for (int32 i = 0; i < CurrentScreen.TimedEvents.Num(); ++i)
	{
		// Skip if already processed
		if (ProcessedTimedEventIndices.Contains(i))
		{
			continue;
		}

		const FStoryTimedEvent& Event = CurrentScreen.TimedEvents[i];

		// Check if it's time to fire this event
		if (ScreenElapsedTime >= Event.StartTime)
		{
			// Mark as processed (Blueprint will query screen state and handle timed events)
			ProcessedTimedEventIndices.Add(i);

			UE_LOG(LogShortStory, Verbose, TEXT("ProcessTimedEvents: Timed event %d reached at time %.2fs: %s '%s'"),
				i, Event.StartTime, *UEnum::GetValueAsString(Event.EventType), *Event.AssetPath);
		}
	}
}

void UShortStorySubsystem::OnScreenTransitionComplete()
{
	// Called when screen transition animation completes in Blueprint
	// For now, immediately start first line of new screen

	if (CurrentScreenIndex >= CurrentStory.Screens.Num())
	{
		return;
	}

	const FStoryScreen& CurrentScreen = CurrentStory.Screens[CurrentScreenIndex];
	if (CurrentScreen.Lines.Num() > 0)
	{
		StartLine(0);
	}
	else
	{
		UE_LOG(LogShortStory, Warning, TEXT("OnScreenTransitionComplete: Screen %d has no lines"), CurrentScreenIndex);
		AdvanceToNextScreen();
	}
}

// ========================================
// Debug Functions
// ========================================

void UShortStorySubsystem::DebugSkipToNextScreen()
{
	if (!bIsPlaying)
	{
		UE_LOG(LogShortStory, Warning, TEXT("DebugSkipToNextScreen: No story is currently playing"));
		return;
	}

	if (CurrentScreenIndex >= CurrentStory.Screens.Num() - 1)
	{
		UE_LOG(LogShortStory, Warning, TEXT("DebugSkipToNextScreen: Already at last screen (%d/%d)"),
			CurrentScreenIndex, CurrentStory.Screens.Num() - 1);
		return;
	}

	UE_LOG(LogShortStory, Display, TEXT("DebugSkipToNextScreen: Skipping from screen %d to %d"),
		CurrentScreenIndex, CurrentScreenIndex + 1);

	AdvanceToNextScreen();
}

void UShortStorySubsystem::DebugSkipToPreviousScreen()
{
	if (!bIsPlaying)
	{
		UE_LOG(LogShortStory, Warning, TEXT("DebugSkipToPreviousScreen: No story is currently playing"));
		return;
	}

	if (CurrentScreenIndex <= 0)
	{
		UE_LOG(LogShortStory, Warning, TEXT("DebugSkipToPreviousScreen: Already at first screen"));
		return;
	}

	UE_LOG(LogShortStory, Display, TEXT("DebugSkipToPreviousScreen: Skipping from screen %d to %d"),
		CurrentScreenIndex, CurrentScreenIndex - 1);

	// Decrement screen index
	CurrentScreenIndex--;

	// Reset screen state
	const FStoryScreen& PrevScreen = CurrentStory.Screens[CurrentScreenIndex];
	CurrentLineIndex = 0;
	ScreenElapsedTime = 0.0f;
	LineElapsedTime = 0.0f;
	PauseElapsedTime = 0.0f;
	ProcessedTimedEventIndices.Empty();
	LineStartTimes.Init(-1.0f, PrevScreen.Lines.Num());

	// Set state to transitioning
	CurrentState = EStoryPlaybackState::TransitioningScreen;
	TransitionElapsedTime = 0.0f;
}

void UShortStorySubsystem::DebugJumpToScreen(int32 ScreenIndex)
{
	if (!bIsPlaying)
	{
		UE_LOG(LogShortStory, Warning, TEXT("DebugJumpToScreen: No story is currently playing"));
		return;
	}

	if (ScreenIndex < 0 || ScreenIndex >= CurrentStory.Screens.Num())
	{
		UE_LOG(LogShortStory, Warning, TEXT("DebugJumpToScreen: Invalid screen index %d (valid range: 0-%d)"),
			ScreenIndex, CurrentStory.Screens.Num() - 1);
		return;
	}

	UE_LOG(LogShortStory, Display, TEXT("DebugJumpToScreen: Jumping from screen %d to %d"),
		CurrentScreenIndex, ScreenIndex);

	// Set screen index
	CurrentScreenIndex = ScreenIndex;

	// Reset screen state
	const FStoryScreen& TargetScreen = CurrentStory.Screens[CurrentScreenIndex];
	CurrentLineIndex = 0;
	ScreenElapsedTime = 0.0f;
	LineElapsedTime = 0.0f;
	PauseElapsedTime = 0.0f;
	ProcessedTimedEventIndices.Empty();
	LineStartTimes.Init(-1.0f, TargetScreen.Lines.Num());

	// Set state to transitioning
	CurrentState = EStoryPlaybackState::TransitioningScreen;
	TransitionElapsedTime = 0.0f;
}

void UShortStorySubsystem::DebugSkipCurrentLine()
{
	if (!bIsPlaying)
	{
		UE_LOG(LogShortStory, Warning, TEXT("DebugSkipCurrentLine: No story is currently playing"));
		return;
	}

	if (CurrentState != EStoryPlaybackState::PlayingLine)
	{
		UE_LOG(LogShortStory, Warning, TEXT("DebugSkipCurrentLine: Not currently playing a line (state: %s)"),
			*UEnum::GetValueAsString(CurrentState));
		return;
	}

	UE_LOG(LogShortStory, Display, TEXT("DebugSkipCurrentLine: Skipping line %d on screen %d"),
		CurrentLineIndex, CurrentScreenIndex);

	// Calculate remaining time for current line
	float RemainingTime = LineDuration - LineElapsedTime;

	// Advance screen time to complete the line (so it appears finished in rendering)
	ScreenElapsedTime += RemainingTime;

	// Force line to complete
	LineElapsedTime = LineDuration;

	// Immediately start pause
	StartPause();
}
