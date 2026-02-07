// Copyright Theory of Magic. All Rights Reserved.

#include "ShortStoryEditor.h"

#define LOCTEXT_NAMESPACE "FShortStoryEditorModule"

void FShortStoryEditorModule::StartupModule()
{
	// Future: Register custom asset editors, validators, etc.
}

void FShortStoryEditorModule::ShutdownModule()
{
	// Future: Unregister custom editors
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FShortStoryEditorModule, ShortStoryEditor)
