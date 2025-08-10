#pragma once

/**
 * hamlib_compat.h - Hamlib compatibility layer for different versions
 * 
 * This header handles compatibility issues between different versions of hamlib
 * Some functions are only available in newer versions and may not be present
 * in older Linux distribution packages.
 */

#include <hamlib/rig.h>

// Check for hamlib version if available
#ifdef HAMLIB_VERSION
  // Version-specific feature detection
  #if HAMLIB_VERSION >= 0x040500  // Version 4.5.0 and later
    #define HAVE_RIG_STOP_VOICE_MEM 1
    #define HAVE_RIG_SPLIT_FREQ_MODE 1
  #endif
#else
  // If version is not available, assume older version
  // We'll disable newer functions for maximum compatibility
  #undef HAVE_RIG_STOP_VOICE_MEM
  #undef HAVE_RIG_SPLIT_FREQ_MODE
#endif

// Manual feature detection based on typical availability
// These functions were added in recent hamlib versions
// Comment out these lines if your hamlib version doesn't have them

// Uncomment if you have a newer hamlib version with these functions:
// #define HAVE_RIG_STOP_VOICE_MEM 1
// #define HAVE_RIG_SPLIT_FREQ_MODE 1

// For maximum compatibility with older distributions, keep these disabled
// Users with newer hamlib can enable them manually

#ifndef HAVE_RIG_STOP_VOICE_MEM
#define HAVE_RIG_STOP_VOICE_MEM 0
#endif

#ifndef HAVE_RIG_SPLIT_FREQ_MODE  
#define HAVE_RIG_SPLIT_FREQ_MODE 0
#endif