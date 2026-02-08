// Copyright Theory of Magic. All Rights Reserved.

#include "ShortStoryParser.h"
#include "ShortStory.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"

bool UShortStoryParser::ParseStoryFile(const FString& StoryFilePath, FShortStory& OutStory, TArray<FString>& OutErrors, int32 MaxLineLength)
{
	OutErrors.Empty();

	// Check if file exists
	if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*StoryFilePath))
	{
		OutErrors.Add(FString::Printf(TEXT("File not found: %s"), *StoryFilePath));
		return false;
	}

	// Load file content
	FString FileContent;
	if (!FFileHelper::LoadFileToString(FileContent, *StoryFilePath))
	{
		OutErrors.Add(FString::Printf(TEXT("Failed to read file: %s"), *StoryFilePath));
		return false;
	}

	// Parse content
	bool bSuccess = ParseStoryFromString(FileContent, OutStory, OutErrors, MaxLineLength);

	if (bSuccess)
	{
		OutStory.SourceFileName = FPaths::GetCleanFilename(StoryFilePath);
	}

	return bSuccess;
}

bool UShortStoryParser::ParseStoryFromString(const FString& StoryText, FShortStory& OutStory, TArray<FString>& OutErrors, int32 MaxLineLength)
{
	OutStory = FShortStory();
	OutErrors.Empty();

	// Split into lines
	TArray<FString> Lines;
	StoryText.ParseIntoArrayLines(Lines);

	if (Lines.Num() == 0)
	{
		OutErrors.Add(TEXT("Empty story file"));
		return false;
	}

	// Parse state
	enum class EParseState
	{
		None,
		StoryMetadata,
		ScreenContent
	};

	EParseState CurrentState = EParseState::None;
	TMap<FString, FString> StoryMetadata;
	TMap<FString, FString> CurrentScreenMetadata;
	FStoryScreen* CurrentScreen = nullptr;
	bool bFoundStorySection = false;

	// Buffer for multi-line blocks
	TArray<FString> PendingLines;

	// Parse line by line
	for (int32 i = 0; i < Lines.Num(); ++i)
	{
		const int32 LineNumber = i + 1;
		FString Line = CleanLine(Lines[i]);

		// Skip empty lines
		if (Line.IsEmpty())
		{
			continue;
		}

		// Check for section header
		FString SectionName;
		if (IsSectionHeader(Line, SectionName))
		{
			// Check for orphaned pending lines
			if (PendingLines.Num() > 0)
			{
				OutErrors.Add(FString::Printf(TEXT("Line %d: Orphaned text lines found before section change (missing metadata line?)"), LineNumber));
				PendingLines.Empty();
			}

			if (SectionName.Equals(TEXT("STORY"), ESearchCase::IgnoreCase))
			{
				CurrentState = EParseState::StoryMetadata;
				bFoundStorySection = true;
				continue;
			}
			else if (SectionName.StartsWith(TEXT("SCREEN"), ESearchCase::IgnoreCase))
			{
				// Finalize previous screen if exists
				CurrentScreen = nullptr;

				// Create new screen
				CurrentState = EParseState::ScreenContent;
				CurrentScreenMetadata.Empty();
				OutStory.Screens.AddDefaulted();
				CurrentScreen = &OutStory.Screens.Last();
				continue;
			}
			else
			{
				OutErrors.Add(FString::Printf(TEXT("Line %d: Unknown section [%s]"), LineNumber, *SectionName));
				continue;
			}
		}

		// Parse based on current state
		switch (CurrentState)
		{
		case EParseState::StoryMetadata:
			{
				if (ParseMetadataLine(Line, StoryMetadata))
				{
					// Metadata parsed successfully
				}
				else
				{
					OutErrors.Add(FString::Printf(TEXT("Line %d: Invalid metadata format: %s"), LineNumber, *Line));
				}
			}
			break;

		case EParseState::ScreenContent:
			{
				if (!CurrentScreen)
				{
					OutErrors.Add(FString::Printf(TEXT("Line %d: No active screen section"), LineNumber));
					continue;
				}

				// Check if it's metadata (before first content line)
				// Exclude [SPACER] from this check
				// We also need to be careful not to mistake a plain text line for metadata if it contains '='? 
				bool bHasContent = CurrentScreen->Lines.Num() > 0 || CurrentScreen->TimedEvents.Num() > 0 || PendingLines.Num() > 0;
				
				if (!bHasContent && ParseMetadataLine(Line, CurrentScreenMetadata)) // Try strict parse
				{
					// Apply screen metadata
					if (CurrentScreenMetadata.Contains(TEXT("background")))
					{
						FString BGPath = CurrentScreenMetadata[TEXT("background")];
						CurrentScreen->BackgroundPath = BGPath;
						
						// Only set soft pointer if it looks like a game asset path
						if (BGPath.StartsWith(TEXT("/Game")) || BGPath.StartsWith(TEXT("/Engine")))
						{
							CurrentScreen->Background = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(BGPath));
						}
					}
					if (CurrentScreenMetadata.Contains(TEXT("transition")))
					{
						EStoryTransition Transition;
						if (ParseTransitionType(CurrentScreenMetadata[TEXT("transition")], Transition))
						{
							CurrentScreen->TransitionType = Transition;
						}
					}
					continue;
				}
				
				// Check if it's a timed event (starts with @)
				if (Line.StartsWith(TEXT("@")))
				{
					FStoryTimedEvent Event;
					FString EventError;
					if (ParseTimedEvent(Line.Mid(1), LineNumber, Event, EventError))
					{
						CurrentScreen->TimedEvents.Add(Event);
					}
					else
					{
						OutErrors.Add(FString::Printf(TEXT("Line %d: %s"), LineNumber, *EventError));
					}
					continue;
				}
				
				// Check for spacer
				if (Line.TrimStartAndEnd().Equals(TEXT("[SPACER]"), ESearchCase::IgnoreCase))
				{
					if (PendingLines.Num() > 0)
					{
						OutErrors.Add(FString::Printf(TEXT("Line %d: [SPACER] found inside a pending text block (missing metadata line?)"), LineNumber));
						PendingLines.Empty();
					}

					FStoryLine SpacerLine;
					SpacerLine.Text = " ";
					SpacerLine.AnimationType = EStoryLineAnimation::Typewriter;
					SpacerLine.PauseDuration = EStoryPauseDuration::None;
					SpacerLine.Effect = EStoryEffect::None;
					CurrentScreen->Lines.Add(SpacerLine);
					continue;
				}

				// Content Logic
				// Check for Pipe | indicating Finisher Line
				if (Line.Contains(TEXT("|")))
				{
					// Finisher Line
					FString FinisherText;
					EStoryLineAnimation Anim;
					EStorySpeed Speed;
					EStoryPauseDuration Pause;
					EStoryEffect Effect;
					FVector2D Offset;
					FString LineError;

					if (ParseLineAttributes(Line, FinisherText, Anim, Speed, Pause, Effect, Offset, LineError))
					{
						// 1. Process Pending Lines
						for (const FString& Pending : PendingLines)
						{
							// Pending lines get: Same Anim/Effect/Offset, but Pause = None
							// Pending lines get: Same Anim/Effect/Offset, but Pause = None
							ProcessTextToLines(Pending, Anim, Speed, EStoryPauseDuration::None, Effect, Offset, CurrentScreen->Lines, MaxLineLength);
						}

						// 2. Process Final Line
						// Gets the actual Pause
						// Gets the actual Pause
						ProcessTextToLines(FinisherText, Anim, Speed, Pause, Effect, Offset, CurrentScreen->Lines, MaxLineLength);

						// 3. Add Paragraph Spacer
						FStoryLine Spacer;
						Spacer.Text = " ";
						Spacer.AnimationType = EStoryLineAnimation::Typewriter;
						Spacer.PauseDuration = EStoryPauseDuration::None;
						Spacer.Effect = EStoryEffect::None;
						CurrentScreen->Lines.Add(Spacer);

						// Clear buffer
						PendingLines.Empty();
					}
					else
					{
						OutErrors.Add(FString::Printf(TEXT("Line %d: %s"), LineNumber, *LineError));
					}
				}
				else
				{
					// Pending Line (Continuation)
					PendingLines.Add(Line);
				}
			}
			break;

		case EParseState::None:
			{
				OutErrors.Add(FString::Printf(TEXT("Line %d: Content found before [STORY] or [SCREEN] section"), LineNumber));
			}
			break;
		}
	}

	// Validate story section was found
	if (!bFoundStorySection)
	{
		OutErrors.Add(TEXT("Missing [STORY] section"));
		return false;
	}

	// Check for leftover pending lines
	if (PendingLines.Num() > 0)
	{
		OutErrors.Add(FString::Printf(TEXT("End of file: Orphaned text lines found (missing metadata line?)")));
	}

	// Apply story metadata
	if (StoryMetadata.Contains(TEXT("title")))
	{
		OutStory.Title = StoryMetadata[TEXT("title")];
	}
	if (StoryMetadata.Contains(TEXT("ost")))
	{
		OutStory.OST = StoryMetadata[TEXT("ost")];
	}

	// Validate story
	if (!OutStory.IsValid())
	{
		if (OutStory.Title.IsEmpty())
		{
			OutErrors.Add(TEXT("Missing 'title' in [STORY] section"));
		}
		if (OutStory.Screens.Num() == 0)
		{
			OutErrors.Add(TEXT("No screens defined (missing [SCREEN_XX] sections)"));
		}
		return false;
	}

	return true;
}

bool UShortStoryParser::ParseMetadataLine(const FString& Line, TMap<FString, FString>& OutMetadata)
{
	// Format: key = value
	// Reject lines containing '|' (those are content lines, not metadata)
	if (Line.Contains(TEXT("|")))
	{
		return false;
	}

	FString Key, Value;
	if (Line.Split(TEXT("="), &Key, &Value))
	{
		Key = Key.TrimStartAndEnd();
		Value = Value.TrimStartAndEnd();

		if (!Key.IsEmpty() && !Value.IsEmpty())
		{
			OutMetadata.Add(Key, Value);
			return true;
		}
	}

	return false;
}

bool UShortStoryParser::ParseLineAttributes(const FString& Line, FString& OutText, EStoryLineAnimation& OutAnim, EStorySpeed& OutSpeed, EStoryPauseDuration& OutPause, EStoryEffect& OutEffect, FVector2D& OutOffset, FString& OutError)
{
	// Format: TEXT | ANIMATION [| key=value | key=value ...]
	// Only TEXT and ANIMATION are mandatory
	// Optional named parameters: speed=X, pause=X, effect=X, offset=X,Y
	TArray<FString> Fields;
	Line.ParseIntoArray(Fields, TEXT("|"), false);

	for (FString& Field : Fields)
	{
		Field = Field.TrimStartAndEnd();
	}

	if (Fields.Num() < 2)
	{
		OutError = FString::Printf(TEXT("Invalid story line format (expected at least 2 fields, got %d)"), Fields.Num());
		return false;
	}

	// Parse text (mandatory)
	OutText = Fields[0];
	if (OutText.TrimStartAndEnd().Equals(TEXT("[SPACER]"), ESearchCase::IgnoreCase))
	{
		OutText = " ";
	}

	if (OutText.IsEmpty())
	{
		OutError = TEXT("Empty text field");
		return false;
	}

	// Parse animation (mandatory)
	if (!ParseAnimationType(Fields[1], OutAnim))
	{
		OutError = FString::Printf(TEXT("Unknown animation type '%s'"), *Fields[1]);
		OutAnim = EStoryLineAnimation::Typewriter;
	}

	// Initialize optional fields with defaults
	OutSpeed = EStorySpeed::Standard;
	OutPause = EStoryPauseDuration::None;
	OutEffect = EStoryEffect::None;
	OutOffset = FVector2D::ZeroVector;

	// Parse optional named parameters (field 2+)
	for (int32 i = 2; i < Fields.Num(); ++i)
	{
		FString Key, Value;
		if (Fields[i].Split(TEXT("="), &Key, &Value))
		{
			Key = Key.TrimStartAndEnd().ToLower();
			Value = Value.TrimStartAndEnd();

			if (Key == TEXT("speed"))
			{
				if (!ParseSpeed(Value, OutSpeed))
				{
					OutError = FString::Printf(TEXT("Unknown speed '%s'"), *Value);
				}
			}
			else if (Key == TEXT("pause"))
			{
				if (!ParsePauseDuration(Value, OutPause))
				{
					OutError = FString::Printf(TEXT("Unknown pause duration '%s'"), *Value);
				}
			}
			else if (Key == TEXT("effect"))
			{
				if (!ParseEffectType(Value, OutEffect))
				{
					OutError = FString::Printf(TEXT("Unknown effect type '%s'"), *Value);
				}
			}
			else if (Key == TEXT("offset"))
			{
				if (!ParsePositionOffset(Value, OutOffset))
				{
					OutError = FString::Printf(TEXT("Invalid offset format '%s' (expected X,Y)"), *Value);
				}
			}
			else
			{
				OutError = FString::Printf(TEXT("Unknown parameter '%s'"), *Key);
			}
		}
		else
		{
			// Not a key=value format, log warning but continue
			OutError = FString::Printf(TEXT("Invalid parameter format '%s' (expected key=value)"), *Fields[i]);
		}
	}

	return true;
}

void UShortStoryParser::ProcessTextToLines(const FString& Text, EStoryLineAnimation Anim, EStorySpeed Speed, EStoryPauseDuration Pause, EStoryEffect Effect, FVector2D Offset, TArray<FStoryLine>& OutLines, int32 MaxLineLength)
{
	// SPLITTING LOGIC
	// 1. Split by Manual Delimiter '\\'
	TArray<FString> ManualSegments;
	Text.ParseIntoArray(ManualSegments, TEXT("\\\\"), false);

	if (ManualSegments.Num() == 0)
	{
		ManualSegments.Add(Text);
	}

	TArray<FString> FinalSegments;
	for (const FString& ManualSeg : ManualSegments)
	{
		// 2. Auto-wrap based on configurable MaxLineLength
		TArray<FString> Wrapped = SplitTextByLength(ManualSeg.TrimStartAndEnd(), MaxLineLength);
		FinalSegments.Append(Wrapped);
	}

	for (int32 i = 0; i < FinalSegments.Num(); ++i)
	{
		FStoryLine NewLine;
		NewLine.Text = FinalSegments[i];
		NewLine.AnimationType = Anim;
		NewLine.Speed = Speed;
		NewLine.Effect = Effect;
		NewLine.PositionOffset = Offset;
		
		// Logic: Intermediate parts of a SINGLE logical line (split manually or wrapped) get None pause.
		// Only the LAST segment of this specific text block gets the requested Pause.
		if (i == FinalSegments.Num() - 1)
		{
			NewLine.PauseDuration = Pause;
		}
		else
		{
			NewLine.PauseDuration = EStoryPauseDuration::LineBreak;
		}

		OutLines.Add(NewLine);
	}
}

// Deprecated Shim
bool UShortStoryParser::ParseStoryLine(const FString& Line, int32 LineNumber, TArray<FStoryLine>& OutLines, FString& OutError)
{
    FString Text;
    EStoryLineAnimation Anim;
    EStorySpeed Speed;
    EStoryPauseDuration Pause;
    EStoryEffect Effect;
    FVector2D Offset;

    if (ParseLineAttributes(Line, Text, Anim, Speed, Pause, Effect, Offset, OutError))
    {
        ProcessTextToLines(Text, Anim, Speed, Pause, Effect, Offset, OutLines, 80); // Use default 80 for deprecated shim
        return true;
    }
    return false;
}

TArray<FString> UShortStoryParser::SplitTextByLength(const FString& Text, int32 MaxLen)
{
	TArray<FString> Result;
	if (Text.IsEmpty()) return Result;
	if (MaxLen <= 0) 
	{
		Result.Add(Text);
		return Result;
	}

	FString Remaining = Text;
	while (Remaining.Len() > MaxLen)
	{
		int32 SplitIndex = -1;
		// Find last space before MaxLen
		for (int32 i = MaxLen; i >= 0; --i)
		{
			if (Remaining[i] == ' ')
			{
				SplitIndex = i;
				break;
			}
		}

		if (SplitIndex == -1)
		{
			// No space found, force split at MaxLen to strictly obey limit.
			SplitIndex = MaxLen;
		}

		Result.Add(Remaining.Left(SplitIndex).TrimStartAndEnd());
		Remaining = Remaining.Mid(SplitIndex).TrimStart(); // Remove leading space if any
	}

	if (!Remaining.IsEmpty())
	{
		Result.Add(Remaining.TrimStartAndEnd());
	}

	return Result;
}

bool UShortStoryParser::ParseTimedEvent(const FString& Line, int32 LineNumber, FStoryTimedEvent& OutEvent, FString& OutError)
{
	// Format: sfx <path> | <time>
	// Format: vfx <path> | <time> | <duration>
	// Format: wait <duration>
	// Format: background <path>

	TArray<FString> Parts;
	Line.ParseIntoArray(Parts, TEXT(" "), true);

	if (Parts.Num() == 0)
	{
		OutError = TEXT("Empty timed event");
		return false;
	}

	FString Command = Parts[0].ToLower();

	// Parse based on command
	if (Command.Equals(TEXT("sfx")))
	{
		// Format: sfx Event:/Path/To/SFX | StartTime
		FString Remainder = Line.Mid(Command.Len()).TrimStartAndEnd();
		TArray<FString> Fields;
		Remainder.ParseIntoArray(Fields, TEXT("|"), false);

		if (Fields.Num() < 2)
		{
			OutError = TEXT("Invalid @sfx format (expected: @sfx <path> | <time>)");
			return false;
		}

		OutEvent.EventType = EStoryTimedEventType::SFX;
		OutEvent.AssetPath = Fields[0].TrimStartAndEnd();
		OutEvent.StartTime = FCString::Atof(*Fields[1].TrimStartAndEnd());
		return true;
	}
	else if (Command.Equals(TEXT("vfx")))
	{
		// Format: vfx BP_Class | StartTime | Duration
		FString Remainder = Line.Mid(Command.Len()).TrimStartAndEnd();
		TArray<FString> Fields;
		Remainder.ParseIntoArray(Fields, TEXT("|"), false);

		if (Fields.Num() < 3)
		{
			OutError = TEXT("Invalid @vfx format (expected: @vfx <class> | <time> | <duration>)");
			return false;
		}

		OutEvent.EventType = EStoryTimedEventType::VFX;
		OutEvent.AssetPath = Fields[0].TrimStartAndEnd();
		OutEvent.StartTime = FCString::Atof(*Fields[1].TrimStartAndEnd());
		OutEvent.Duration = FCString::Atof(*Fields[2].TrimStartAndEnd());
		return true;
	}
	else if (Command.Equals(TEXT("wait")))
	{
		// Format: wait <duration>
		if (Parts.Num() < 2)
		{
			OutError = TEXT("Invalid @wait format (expected: @wait <duration>)");
			return false;
		}

		OutEvent.EventType = EStoryTimedEventType::Wait;
		OutEvent.StartTime = FCString::Atof(*Parts[1]);
		return true;
	}
	else if (Command.Equals(TEXT("background")))
	{
		// Format: background /Game/Textures/Path
		FString Path = Line.Mid(Command.Len()).TrimStartAndEnd();
		if (Path.IsEmpty())
		{
			OutError = TEXT("Invalid @background format (expected: @background <path>)");
			return false;
		}

		OutEvent.EventType = EStoryTimedEventType::BackgroundChange;
		OutEvent.AssetPath = Path;
		return true;
	}
	else
	{
		OutError = FString::Printf(TEXT("Unknown timed event command '%s'"), *Command);
		return false;
	}
}

bool UShortStoryParser::ParseAnimationType(const FString& AnimString, EStoryLineAnimation& OutAnim)
{
	FString Lower = AnimString.ToLower();

	if (Lower.Equals(TEXT("typewriter")) || Lower.Equals(TEXT("standard")) || Lower.Equals(TEXT("alone")) || Lower.Equals(TEXT("slow")) || Lower.Equals(TEXT("fast")))
	{
		OutAnim = EStoryLineAnimation::Typewriter;
		return true;
	}
	else if (Lower.Equals(TEXT("left_to_right")) || Lower.Equals(TEXT("lefttoright")))
	{
		OutAnim = EStoryLineAnimation::LeftToRight;
		return true;
	}
	else if (Lower.Equals(TEXT("top_down")) || Lower.Equals(TEXT("topdown")))
	{
		OutAnim = EStoryLineAnimation::TopDown;
		return true;
	}
	else if (Lower.Equals(TEXT("word_rain")) || Lower.Equals(TEXT("wordrain")))
	{
		OutAnim = EStoryLineAnimation::WordRain;
		return true;
	}
	else if (Lower.Equals(TEXT("snake")))
	{
		OutAnim = EStoryLineAnimation::Snake;
		return true;
	}
	else if (Lower.Equals(TEXT("slow")))
	{
		// Map slow/fast to Typewriter for now as they are just speed variations, 
		// or maybe we should keep them if we add them back to enum?
		// User removed Slow/Fast from enum.
		OutAnim = EStoryLineAnimation::Typewriter; 
		return true;
	}
	else if (Lower.Equals(TEXT("fast")))
	{
		OutAnim = EStoryLineAnimation::Typewriter;
		return true;
	}
	else if (Lower.Equals(TEXT("paragraph")) || Lower.Equals(TEXT("fade_in")) || Lower.Equals(TEXT("fadein")))
	{
		OutAnim = EStoryLineAnimation::Paragraph;
		return true;
	}

	return false;
}

bool UShortStoryParser::ParsePauseDuration(const FString& PauseString, EStoryPauseDuration& OutPause)
{
	FString Lower = PauseString.ToLower();

	// Check for numeric value (including 0)
	if (Lower.Equals(TEXT("0")) || Lower.Equals(TEXT("none")))
	{
		OutPause = EStoryPauseDuration::None;
		return true;
	}
	else if (Lower.Equals(TEXT("short")))
	{
		OutPause = EStoryPauseDuration::Short;
		return true;
	}
	else if (Lower.Equals(TEXT("standard")))
	{
		OutPause = EStoryPauseDuration::Standard;
		return true;
	}
	else if (Lower.Equals(TEXT("long")))
	{
		OutPause = EStoryPauseDuration::Long;
		return true;
	}

	return false;
}

bool UShortStoryParser::ParseEffectType(const FString& EffectString, EStoryEffect& OutEffect)
{
	FString Lower = EffectString.ToLower();

	if (Lower.Equals(TEXT("none")))
	{
		OutEffect = EStoryEffect::None;
		return true;
	}
	else if (Lower.Equals(TEXT("shake_low")) || Lower.Equals(TEXT("shakelow")))
	{
		OutEffect = EStoryEffect::ShakeLow;
		return true;
	}
	else if (Lower.Equals(TEXT("shake_med")) || Lower.Equals(TEXT("shakemed")) || Lower.Equals(TEXT("shake_medium")))
	{
		OutEffect = EStoryEffect::ShakeMed;
		return true;
	}
	else if (Lower.Equals(TEXT("shake_high")) || Lower.Equals(TEXT("shakehigh")))
	{
		OutEffect = EStoryEffect::ShakeHigh;
		return true;
	}
	else if (Lower.Equals(TEXT("storm")))
	{
		OutEffect = EStoryEffect::Storm;
		return true;
	}

	return false;
}

bool UShortStoryParser::ParseTransitionType(const FString& TransitionString, EStoryTransition& OutTransition)
{
	FString Lower = TransitionString.ToLower();

	if (Lower.Equals(TEXT("instant")))
	{
		OutTransition = EStoryTransition::Instant;
		return true;
	}
	else if (Lower.Equals(TEXT("fade")))
	{
		OutTransition = EStoryTransition::Fade;
		return true;
	}
	else if (Lower.Equals(TEXT("crossfade")))
	{
		OutTransition = EStoryTransition::Crossfade;
		return true;
	}

	return false;
}

bool UShortStoryParser::ParsePositionOffset(const FString& OffsetString, FVector2D& OutOffset)
{
	// Format: X,Y (e.g. "0,20" or "-50,0")
	TArray<FString> Components;
	OffsetString.ParseIntoArray(Components, TEXT(","), false);

	if (Components.Num() == 2)
	{
		float X = FCString::Atof(*Components[0].TrimStartAndEnd());
		float Y = FCString::Atof(*Components[1].TrimStartAndEnd());
		OutOffset = FVector2D(X, Y);
		return true;
	}

	return false;
}

FString UShortStoryParser::CleanLine(const FString& Line)
{
	// Trim whitespace
	FString Cleaned = Line.TrimStartAndEnd();

	// Remove comments (lines starting with #)
	if (Cleaned.StartsWith(TEXT("#")))
	{
		return FString();
	}

	return Cleaned;
}

bool UShortStoryParser::IsSectionHeader(const FString& Line, FString& OutSectionName)
{
	// Format: [SECTION_NAME]
	if (Line.StartsWith(TEXT("[")) && Line.EndsWith(TEXT("]")))
	{
		OutSectionName = Line.Mid(1, Line.Len() - 2).TrimStartAndEnd();
		
		// EXCEPTION: [SPACER] is a content token, not a section header
		if (OutSectionName.Equals(TEXT("SPACER"), ESearchCase::IgnoreCase))
		{
			return false;
		}

		return !OutSectionName.IsEmpty();
	}

	return false;
}

bool UShortStoryParser::ParseSpeed(const FString& SpeedString, EStorySpeed& OutSpeed)
{
	FString Lower = SpeedString.ToLower();

	if (Lower.Equals(TEXT("standard")))
	{
		OutSpeed = EStorySpeed::Standard;
		return true;
	}
	else if (Lower.Equals(TEXT("fast")))
	{
		OutSpeed = EStorySpeed::Fast;
		return true;
	}
	else if (Lower.Equals(TEXT("slow")))
	{
		OutSpeed = EStorySpeed::Slow;
		return true;
	}
	return false;
}
