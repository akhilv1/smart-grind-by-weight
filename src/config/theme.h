#pragma once

//==============================================================================
// USER INTERFACE THEME CONFIGURATION
//==============================================================================
// This file contains all user interface theming and visual design constants
// for the LVGL-based touchscreen interface. These values define the visual
// appearance, colors, dimensions, and styling of all UI elements.

//------------------------------------------------------------------------------
// COLOR SCHEME (RGB565 Format)
//------------------------------------------------------------------------------
// Primary brand colors
#define THEME_COLOR_PRIMARY 0xFF453A                                           // Primary theme color (red): grind action
#define THEME_COLOR_ACCENT 0x0A84FF                                            // Accent color for highlights (blue); white text on it reads at 28px+
#define THEME_COLOR_SECONDARY 0xAAAAAA                                         // Secondary theme color (light gray)

// Text colors
#define THEME_COLOR_TEXT_PRIMARY 0xFFFFFF                                      // Primary text color (white)
#define THEME_COLOR_TEXT_SECONDARY 0xCCCCCC                                    // Secondary text color (light gray)
#define THEME_COLOR_TEXT_TERTIARY 0x8C8C8C                                     // Section labels and captions (dimmer than body text)

// Background colors
#define THEME_COLOR_BACKGROUND 0x000000                                        // Background color (black)
#define THEME_COLOR_SURFACE 0x1C1C1E                                           // Cards: Settings rows, segmented track, neutral action buttons
#define THEME_COLOR_SURFACE_PRESSED 0x2C2C2E                                   // A card while pressed
#define THEME_COLOR_TRACK 0x3A3A3C                                             // Slider track and off-toggle track (sits on a card)
#define THEME_COLOR_NEUTRAL 0x666666                                           // Neutral fill for round actions on bare black (Menu gear, TARE, flip pill)
#define THEME_COLOR_BACKGROUND_MOCK 0x035e03                                   // Background color when mock hardware is active (dark green)

// Status indication colors
#define THEME_COLOR_SUCCESS 0x30D158                                           // Success state color (green)
#define THEME_COLOR_ERROR 0xFF453A                                             // Error state color (red)
#define THEME_COLOR_WARNING 0xFF9F0A                                           // Warning state color (orange)
#define THEME_COLOR_DETECTING 0xFFD60A                                         // AUTO tab: reading a portafilter placement (yellow)
#define THEME_COLOR_GRINDER_ACTIVE 0x403800                                    // Grinder active indicator (dark yellow)

// Settings controls
#define THEME_COLOR_SELECTED THEME_COLOR_ACCENT                                // On toggles / selected segments (blue)
#define THEME_COLOR_SECTION_LABEL THEME_COLOR_TEXT_TERTIARY                    // Section labels
#define THEME_COLOR_HAIRLINE 0x3A3A3C                                          // Divider rule inside a page

//------------------------------------------------------------------------------
// TYPOGRAPHY ROLES (Settings and dialogs)
//------------------------------------------------------------------------------
// Type scale based on watchOS (Apple HIG, Dynamic Type "Large (default 40mm/41mm/42mm)").
// Apple Watch 41mm and this panel are both ~326 ppi, so 1 watchOS pt = 2 px on the watch;
// scaled by 280/352 for our narrower panel that is x1.6, then rounded a step down because
// Montserrat runs wider and taller than SF Compact. Nothing the user must read is under 20 px.
//   watchOS role     pt   x1.6   ours
//   Large Title      36   57.6   56   THEME_FONT_DISPLAY_VALUE (home tab value, live weight)
//   Title 3          19   30.4   28   THEME_FONT_TITLE, THEME_FONT_DISPLAY_NAME, THEME_FONT_SYMBOL
//   Headline / Body  16   25.6   24   THEME_FONT_ROW
//   Caption 1        15   24.0   22   THEME_FONT_BODY
//   Caption 2        14   22.4   20   THEME_FONT_SECTION, THEME_FONT_NAV
//   Footnote 1       13   20.8   20   THEME_FONT_CAPTION, THEME_FONT_STATUS
#define THEME_FONT_TITLE (&lv_font_montserrat_28)                              // Dialog headlines (Title 3)
#define THEME_FONT_ROW (&lv_font_montserrat_24)                                // Tappable rows, segments, button labels (Body)
#define THEME_FONT_BODY (&lv_font_montserrat_22)                               // Descriptions, data rows, messages (Caption 1)
#define THEME_FONT_SECTION (&lv_font_montserrat_20)                            // Sentence-case section labels, tertiary color (Caption 2)
#define THEME_FONT_CAPTION (&lv_font_montserrat_20)                            // Secondary line of a two-line row, tertiary color (Footnote 1)
#define THEME_FONT_NAV (&lv_font_montserrat_20)                                // Global menubar title, back arrow and status icons (Caption 2)
#define THEME_FONT_STATUS (&lv_font_montserrat_20)                             // Status line under a display value: AUTO tab, grinding target, OTA (Footnote 1)
#define THEME_FONT_SYMBOL (&lv_font_montserrat_28)                             // Glyph-only buttons: OK, PLUS, MINUS, PLAY in the AUTO ring (Title 3)
#define THEME_FONT_DISPLAY_NAME (&lv_font_montserrat_28)                       // Profile name over a display value, screen headlines on arc/chart/OTA (Title 3)
#define THEME_FONT_DISPLAY_VALUE (&lv_font_montserrat_56)                      // Home tab target and live weight readouts (Large Title; kept legible at arm's length)

//------------------------------------------------------------------------------
// SETTINGS LAYOUT
//------------------------------------------------------------------------------
#define THEME_CONTENT_WIDTH_PX 260                                             // Every Settings element (10px gutters)
#define THEME_ROW_HEIGHT_PX 64                                                 // Toggles, segmented controls, menu items
#define THEME_ROW_GAP_PX 10                                                    // Between consecutive rows
#define THEME_ROW_INSET_PX 20                                                  // Text inset inside a row
#define THEME_SECTION_GAP_PX 20                                                // Space above a section label
#define THEME_SECTION_LABEL_GAP_PX 6                                           // Space below a section label
#define THEME_SEGMENT_TRACK_PAD_PX 4                                           // Inset of a segment inside the segmented control track
#define THEME_SWITCH_WIDTH_PX 56                                               // Toggle track size
#define THEME_SWITCH_HEIGHT_PX 32
#define THEME_SWITCH_KNOB_INSET_PX 4                                           // Knob inset from the track edge (knob = height - 2*inset = 24)
#define THEME_DESCRIPTION_GAP_PX 6                                             // Description under its control

//------------------------------------------------------------------------------
// UI ELEMENT DIMENSIONS
//------------------------------------------------------------------------------
// Button specifications
#define THEME_BUTTON_WIDTH_PX 120                                             // Standard button width

// Progress and feedback elements
#define THEME_PROGRESS_ARC_DIAMETER_PX 200                                    // Progress arc diameter

// General layout
#define THEME_CORNER_RADIUS_PX 20                                             // Standard UI element corner radius

// Global menubar: persistent top bar (home button + status icons). Non-immersive
// screens inset their content below it by this height (see layout_below_menubar()).
#define UI_MENUBAR_HEIGHT_PX 44                                                // Height of the global top menubar

//------------------------------------------------------------------------------
// OPACITY VALUES
//------------------------------------------------------------------------------
#define THEME_OPACITY_OVERLAY 204                                             // Overlay background opacity (80% of 255)