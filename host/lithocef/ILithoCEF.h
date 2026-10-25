/* ILithoCEF.h — compatibility name for the hardware layer (IHwLayer).
 *
 * The ILithoCEF contract was renamed and split on 2026-10-25 (JD): the
 * base is IHwLayer (IHwLayer.h) and the CEF tenant is IHwCef (IHwCef.h).
 * This header keeps the old spellings compiling for out-of-tree callers;
 * new code includes IHwLayer.h / IHwCef.h directly.
 */
#pragma once

#include "IHwCef.h"

#define LITHOCEF_ABI_VERSION LITHOHW_ABI_VERSION
typedef LithoHwShell LithoCefShell;
typedef LithoHwResult LithoCefResult;
typedef LithoHwSurfaceKind LithoCefSurfaceKind;
typedef LithoHwSurfaceDesc LithoCefSurfaceDesc;
typedef LithoHwConfig LithoCefConfig;
typedef LithoHwEngineBoot LithoCefEngineBoot;
typedef LithoHwInputEventKind LithoCefInputEventKind;
typedef LithoHwInputEvent LithoCefInputEvent;
typedef LithoHwInputSink LithoCefInputSink;
typedef LithoHwApi LithoCefApi;
typedef LithoHwCensus LithoCefCensus;
#define LITHOCEF_OK LITHOHW_OK
#define LITHOCEF_ERR_INVALID LITHOHW_ERR_INVALID
#define LITHOCEF_ERR_UNSUPPORTED LITHOHW_ERR_UNSUPPORTED
#define LITHOCEF_ERR_NOT_BOOTED LITHOHW_ERR_NOT_BOOTED
#define LITHOCEF_ERR_INTERNAL LITHOHW_ERR_INTERNAL
#define LITHOCEF_SURFACE_NONE LITHOHW_SURFACE_NONE
#define LITHOCEF_SURFACE_NATIVE_WINDOW LITHOHW_SURFACE_NATIVE_WINDOW
#define LITHOCEF_SURFACE_OFFSCREEN_TEXTURE LITHOHW_SURFACE_OFFSCREEN_TEXTURE
#define LITHOCEF_INPUT_KEY LITHOHW_INPUT_KEY
#define LITHOCEF_INPUT_POINTER_MOVE LITHOHW_INPUT_POINTER_MOVE
#define LITHOCEF_INPUT_POINTER_BUTTON LITHOHW_INPUT_POINTER_BUTTON
#define LITHOCEF_INPUT_WHEEL LITHOHW_INPUT_WHEEL
#define LITHOCEF_INPUT_TEXT LITHOHW_INPUT_TEXT
#define LITHOCEF_INPUT_FOCUS LITHOHW_INPUT_FOCUS
#define lithocef_get_api lithohw_get_api