// Copyright Theory of Magic. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ShortStoryStructs.h"
#include "ShortStory.h"
#include "ShortStoryParser.generated.h"

/**
 * Static utility class for parsing .tos (Theory of Magic Story) files
 *
 * File Format:
 * [STORY]
 * title = Story Title
 * ost = Event:/Music/Path
 *
 * [SCREEN_01]
 * background = /Game/Textures/Path
 * transition = fade
 *
 * TEXT | ANIMATION | PAUSE | EFFECT [| OFFSET_X,OFFSET_Y]
 * @sfx Event:/SFX/Path | StartTime
 * @vfx BP_ParticleClass | StartTime | Duration
 */
UCLASS()
class SHORTSTORY_API UShortStoryParser : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Parse a .tos file from disk
	 * @param StoryFilePath Absolute path to .tos file
	 * @param OutStory Parsed story data
	 * @param OutErrors Array of error messages with line numbers
	 * @param MaxLineLength Maximum character count for auto-wrapping text
	 * @return True if parsing succeeded (OutStory is valid)
	 */
	static bool ParseStoryFile(const FString& StoryFilePath, FShortStory& OutStory, TArray<FString>& OutErrors, int32 MaxLineLength = 80);

	/**
	 * Parse a .tos format string
	 * @param StoryText Raw text content
	 * @param OutStory Parsed story data
	 * @param OutErrors Array of error messages with line numbers
	 * @param MaxLineLength Maximum character count for auto-wrapping text
	 * @return True if parsing succeeded (OutStory is valid)
	 */
	static bool ParseStoryFromString(const FString& StoryText, FShortStory& OutStory, TArray<FString>& OutErrors, int32 MaxLineLength = 80);

	static bool ParseStoryLine(const FString& Line, int32 LineNumber, TArray<FStoryLine>& OutLines, FString& OutError);

private:
	/**
	 * Parse a metadata line (key = value format)
	 * @param Line Raw line text
	 * @param OutMetadata Map to store key-value pairs
	 * @return True if line is valid metadata
	 */
	static bool ParseMetadataLine(const FString& Line, TMap<FString, FString>& OutMetadata);

	/**
	 * Parse a story line string into its components (Text + Metadata)
	 * @param Line Raw line text
	 * @param OutText The text content
	 * @param OutAnim Animation type
	 * @param OutPause Pause duration
	 * @param OutEffect Visual effect
	 * @param OutOffset Position offset
	 * @param OutError Error message
	 * @return True if valid
	 */
	static bool ParseLineAttributes(const FString& Line, FString& OutText, EStoryLineAnimation& OutAnim, EStorySpeed& OutSpeed, EStoryPauseDuration& OutPause, EStoryEffect& OutEffect, FVector2D& OutOffset, FString& OutError);

	/**
	 * Process a text string into final story lines (handling wrapping, spacers, etc.)
	 * @param Text Content text
	 * @param Anim Animation to apply
	 * @param Pause Pause to apply (usually only to the last part)
	 * @param Effect Effect to apply
	 * @param Offset Offset to apply
	 * @param OutLines Resulting lines
	 * @param MaxLineLength Maximum character count for auto-wrapping text
	 */
	static void ProcessTextToLines(const FString& Text, EStoryLineAnimation Anim, EStorySpeed Speed, EStoryPauseDuration Pause, EStoryEffect Effect, FVector2D Offset, TArray<FStoryLine>& OutLines, int32 MaxLineLength = 80);

	/**
	 * Helper to split text into chunks respecting word boundaries
	 * @param Text Input text
	 * @param MaxLen Maximum length per chunk
	 * @return Array of text chunks
	 */
	static TArray<FString> SplitTextByLength(const FString& Text, int32 MaxLen);

	/**
	 * Parse a timed event command (@sfx, @vfx, @wait, @background)
	 * @param Line Raw line text (without @ prefix)
	 * @param LineNumber Line number in file (for error reporting)
	 * @param OutEvent Parsed event data
	 * @param OutError Error message if parsing fails
	 * @return True if line is valid timed event
	 */
	static bool ParseTimedEvent(const FString& Line, int32 LineNumber, FStoryTimedEvent& OutEvent, FString& OutError);

	/**
	 * Parse animation type from string
	 * @param AnimString String representation (e.g. "left_to_right")
	 * @param OutAnim Parsed enum value
	 * @return True if valid animation type
	 */
	static bool ParseAnimationType(const FString& AnimString, EStoryLineAnimation& OutAnim);

	/**
	 * Parse pause duration from string
	 * @param PauseString String representation (e.g. "short", "1.5", "0")
	 * @param OutPause Parsed enum value
	 * @return True if valid pause duration
	 */
	static bool ParsePauseDuration(const FString& PauseString, EStoryPauseDuration& OutPause);
	static bool ParseSpeed(const FString& SpeedString, EStorySpeed& OutSpeed);

	/**
	 * Parse effect type from string
	 * @param EffectString String representation (e.g. "shake_high")
	 * @param OutEffect Parsed enum value
	 * @return True if valid effect type
	 */
	static bool ParseEffectType(const FString& EffectString, EStoryEffect& OutEffect);

	/**
	 * Parse transition type from string
	 * @param TransitionString String representation (e.g. "fade")
	 * @param OutTransition Parsed enum value
	 * @return True if valid transition type
	 */
	static bool ParseTransitionType(const FString& TransitionString, EStoryTransition& OutTransition);

	/**
	 * Parse position offset from string (e.g. "0,20" or "-50,0")
	 * @param OffsetString String representation
	 * @param OutOffset Parsed vector
	 * @return True if valid offset format
	 */
	static bool ParsePositionOffset(const FString& OffsetString, FVector2D& OutOffset);

	/**
	 * Trim whitespace and remove comments from line
	 * @param Line Raw line text
	 * @return Cleaned line
	 */
	static FString CleanLine(const FString& Line);

	/**
	 * Check if line is a section header (e.g. [STORY], [SCREEN_01])
	 * @param Line Raw line text
	 * @param OutSectionName Section name without brackets
	 * @return True if line is a section header
	 */
	static bool IsSectionHeader(const FString& Line, FString& OutSectionName);
};
