# ShortStory Plugin

A standalone narrative system for presenting visual novel-style story sequences with rich text formatting, animations, and audio integration.

## Features

- **Story File Format (.tos)**: Custom text-based format for defining stories with screens, text, and timed events
- **Text Animations**: 6 animation types (Typewriter, LeftToRight, Paragraph, TopDown, WordRain, Snake)
- **Configurable Timing**: CSV-based speed configurations (Standard, Fast, Slow)
- **Runtime Texture Loading**: Load background images from disk at runtime
- **FMOD Integration**: Background music and sound effects
- **Visual Effects**: Screen shake, storm effects, and custom VFX
- **Console Commands**: Debug commands for testing and development

## Installation

This plugin is included in Theory of Magic. To use in another project:

1. Copy the `Plugins/ShortStory` directory to your project's `Plugins` folder
2. Enable the plugin in your `.uproject` file or via the Plugins UI
3. Ensure FMODStudio plugin is enabled (required dependency)

## Content Structure

```
Plugins/ShortStory/Content/
├── Stories/
│   ├── Config/
│   │   ├── Speed_Standard.csv
│   │   ├── Speed_Fast.csv
│   │   ├── Speed_Slow.csv
│   │   └── ShortStoryGlobal.csv
│   ├── TestStory.tos
│   └── Orazio/
│       └── Orazio.tos
└── Examples/
    └── (Blueprint examples)
```

## Story File Format (.tos)

```
[STORY]
title = My Story Title
ost = Event:/Music/MyBackgroundMusic

[SCREEN_01]
background = /Plugin/ShortStory/Content/Stories/Orazio/bg01.png
transition = fade

Once upon a time... | typewriter | standard | none
There was a magical world. | typewriter | slow | long

@sfx Event:/SFX/Thunder | 1.5
@background /Plugin/ShortStory/Content/Stories/Orazio/bg02.png | 3.0

[SCREEN_02]
...
```

## Console Commands

- `Story.List` - List all available story files
- `Story.Load <filename.tos>` - Load and parse a story file
- `Story.ClearCache` - Clear all cached stories
- `Story.NextScreen` - [DEBUG] Skip to next screen
- `Story.PrevScreen` - [DEBUG] Go to previous screen
- `Story.JumpToScreen <index>` - [DEBUG] Jump to specific screen
- `Story.SkipLine` - [DEBUG] Skip current line

## Blueprint Usage

```cpp
// Get the subsystem
UShortStorySubsystem* Subsystem = GetGameInstance()->GetSubsystem<UShortStorySubsystem>();

// Start playing a story
Subsystem->StartStory("Orazio.tos");

// In widget Tick: Get current screen state
FStoryScreenState State = Subsystem->GetCurrentScreenState();
```

## Dependencies

- **FMODStudio** (Required) - For audio playback
- **ImageWrapper** - For runtime image loading
- **GameplayTags** - For tagging system
- **Projects** - For plugin manager access

## FMOD Audio

Audio events remain in the main project's FMOD banks. The plugin references FMOD events by path (e.g., `Event:/OST/ShortStory`).

## License

Copyright Theory of Magic. All Rights Reserved.
