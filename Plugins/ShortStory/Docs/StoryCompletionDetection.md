# ShortStory Completion Detection - Usage Guide

## Overview
ShortStory now provides **two methods** for detecting when a story finishes:
1. **Event-Based (Recommended)**: Use event dispatchers for automatic callbacks
2. **Polling-Based (Legacy)**: Check state manually each frame

Both methods are fully supported and can be used simultaneously if needed.

---

## Method 1: Event-Based Detection (Recommended)

### Story Completion Events

**Subscribe in Blueprint:**
1. Get the `ShortStorySubsystem` reference
2. Find the `On Story Completed` event (red event pin)
3. Bind your custom event to it
4. Your event will fire automatically when the story finishes

**Blueprint Example:**
```
Event Begin Play
├─ Get Game Instance
├─ Get Subsystem (ShortStorySubsystem)
├─ Bind Event to On Story Completed
│  └─ Custom Event: Handle Story Completed
│     └─ Hide Widget
│     └─ Load Next Level
```

**C++ Example:**
```cpp
void UMyWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (UShortStorySubsystem* Subsystem = GetGameInstance()->GetSubsystem<UShortStorySubsystem>())
    {
        // Bind to completion event
        Subsystem->OnStoryCompleted.AddDynamic(this, &UMyWidget::HandleStoryCompleted);
    }
}

void UMyWidget::HandleStoryCompleted()
{
    // Story finished - hide UI, trigger next level, etc.
    RemoveFromParent();
}
```

### Screen Change Events

Get notified when advancing to a new screen:

**C++ Example:**
```cpp
void UMyWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (UShortStorySubsystem* Subsystem = GetGameInstance()->GetSubsystem<UShortStorySubsystem>())
    {
        // Bind to screen change event
        Subsystem->OnScreenChanged.AddDynamic(this, &UMyWidget::HandleScreenChanged);
    }
}

void UMyWidget::HandleScreenChanged(int32 NewScreenIndex)
{
    // New screen started - update UI, play sound, etc.
    UE_LOG(LogTemp, Log, TEXT("Advanced to screen %d"), NewScreenIndex);
}
```

---

## Method 2: Polling-Based Detection (Legacy)

### Check Playback State
**File:** `ShortStorySubsystem.h:141-142`

```cpp
UFUNCTION(BlueprintPure, Category = "Narrative|Story Playback")
EStoryPlaybackState GetPlaybackState() const;
```

**C++ Usage:**
```cpp
if (ShortStorySubsystem->GetPlaybackState() == EStoryPlaybackState::Completed)
{
    // Story has finished
}
```

**Blueprint:** Call `Get Playback State` and compare to `Completed` enum value.

---

### Check Screen State
**File:** `ShortStorySubsystem.h:160-161`

```cpp
UFUNCTION(BlueprintPure, Category = "Narrative|Story Playback")
FStoryScreenState GetCurrentScreenState() const;
```

**C++ Usage:**
```cpp
FStoryScreenState ScreenState = ShortStorySubsystem->GetCurrentScreenState();
if (ScreenState.bIsComplete)
{
    // Story has finished
}
```

**Blueprint:** Call `Get Current Screen State` and check the `Is Complete` boolean field.

---

### Combined Check (Most Reliable for Polling)
```cpp
if (!ShortStorySubsystem->IsPlaying() &&
    ShortStorySubsystem->GetPlaybackState() == EStoryPlaybackState::Completed)
{
    // Story definitively finished
}
```

---

## EStoryPlaybackState Enum Values
Located in `ShortStorySubsystem.h:14-22`:
- `Idle` - No story loaded
- `PlayingLine` - Currently displaying a line
- `PausingAfterLine` - Pause between lines
- `TransitioningScreen` - Moving to next screen
- `Completed` - **Story finished**

---

## Implementation Details

### Event Broadcast Timing
- `OnStoryCompleted` broadcasts in `AdvanceToNextScreen()` when all screens are exhausted
- `OnScreenChanged` broadcasts when transitioning to a new screen (excluding the first screen)
- Events are broadcast **after** state changes but **before** the next screen starts

**Code locations:**
- Story completion: `ShortStorySubsystem.cpp:1191` in `AdvanceToNextScreen()`
- Screen change: `ShortStorySubsystem.cpp:1173` in `AdvanceToNextScreen()`

### State Management
When the story reaches the last screen and advances past it:
- `bIsPlaying = false`
- `CurrentState = EStoryPlaybackState::Completed`
- Ticker is unregistered (no more updates)
- `OnStoryCompleted` event is broadcast

---

## Polling Pattern Example (Blueprint)

If you prefer polling over events:

1. Create an Event Tick or Timer in your widget/actor
2. Call `Get Playback State`
3. Branch on `== Completed`
4. Execute completion logic (hide UI, trigger next level, etc.)
5. Optionally stop checking after completion detected

**Note:** Event-based detection is more efficient and cleaner, but polling is still fully supported for backward compatibility.

---

## Best Practices

### Use Events When:
- You want clean, decoupled code
- You need to trigger one-time actions (hide UI, load next level)
- You're building new features

### Use Polling When:
- You need to continuously monitor state (progress bars, debug displays)
- You're already polling for other state (screen state, line progress)
- You're maintaining legacy code that uses polling

### Combine Both When:
- You need both one-time triggers (events) and continuous updates (polling)
- Example: Use `OnScreenChanged` for screen transitions, but poll `GetCurrentScreenState()` for rendering text

---

## Migration Guide

### From Polling to Events

**Old Code (Polling):**
```cpp
void UMyWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (ShortStorySubsystem->GetPlaybackState() == EStoryPlaybackState::Completed)
    {
        if (!bHasHandledCompletion)
        {
            HandleStoryCompleted();
            bHasHandledCompletion = true;
        }
    }
}
```

**New Code (Events):**
```cpp
void UMyWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // One-time binding - no need for flags or tick checks
    if (UShortStorySubsystem* Subsystem = GetGameInstance()->GetSubsystem<UShortStorySubsystem>())
    {
        Subsystem->OnStoryCompleted.AddDynamic(this, &UMyWidget::HandleStoryCompleted);
    }
}

void UMyWidget::HandleStoryCompleted()
{
    // Called automatically once when story completes
    RemoveFromParent();
}
```

### Benefits of Migration:
- No need for `bHasHandledCompletion` flags
- No tick overhead for checking state
- Cleaner, more maintainable code
- Automatic cleanup when subsystem is destroyed

---

## Troubleshooting

### Event Not Firing?
1. Check that you bound the event before the story completed
2. Verify the subsystem is valid when binding
3. Ensure your object is still alive when the event fires
4. Check that `OnStoryCompleted.Broadcast()` is being called (add breakpoint or log)

### Event Firing Multiple Times?
- This shouldn't happen - the event only broadcasts once when transitioning past the last screen
- If it does, check that you're not calling `StartStory()` multiple times
- Verify you're not binding the same delegate multiple times

### Want Both Polling and Events?
- Totally fine! Use events for one-time actions and polling for continuous state updates
- Example: Event for "hide widget when done", polling for "update progress bar each frame"

---

## Summary

| Feature | Event-Based | Polling-Based |
|---------|------------|---------------|
| **Efficiency** | High (no per-frame cost) | Low (checks every frame) |
| **Complexity** | Low (bind and forget) | Medium (need flags/checks) |
| **Use Case** | One-time actions | Continuous monitoring |
| **Backward Compat** | New (2025) | Always available |
| **Recommended** | ✅ Yes | Only if needed |

**Bottom Line:** Use events for most cases. Poll only when you need continuous state monitoring.
