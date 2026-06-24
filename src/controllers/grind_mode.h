#pragma once

enum class GrindMode {
    WEIGHT = 0,
    TIME = 1,
    CALIBRATED_TIME = 2,   // "Hybrid": calibrated time grind
    MANUAL = 3,            // Hold-to-grind (available in both feed modes)
    SINGLE_AUTO = 4        // Single-dose auto-stop when flow drops to ~0
};

// Top-level feed mode: how beans are supplied to the grinder.
// Hopper -> Weight / Time / Hybrid / Manual
// Single -> Auto (SINGLE_AUTO) / Manual
enum class FeedMode {
    HOPPER = 0,
    SINGLE = 1
};

