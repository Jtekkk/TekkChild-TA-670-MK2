#pragma once

/*
    Module selection for the split build.

    The TekkChild TC-670 is built from four modules: the Vari-Mu compressor, the
    Tape Brain colour section, the TEKKVISION stereo imager and the 1176 limiter.
    A build target may compile just one of them into a standalone plugin by
    defining TEKK_MODULE_SELECTED plus the matching TEKK_HAS_* flag (and a name).
    With nothing defined we build the original all-in-one unit, so the combined
    plugin keeps working unchanged.
*/

#if ! defined (TEKK_MODULE_SELECTED)
 #define TEKK_HAS_COMP    1
 #define TEKK_HAS_TAPE    1
 #define TEKK_HAS_IMAGER  1
 #define TEKK_HAS_LIMITER 1
#endif

#ifndef TEKK_HAS_COMP
 #define TEKK_HAS_COMP 0
#endif
#ifndef TEKK_HAS_TAPE
 #define TEKK_HAS_TAPE 0
#endif
#ifndef TEKK_HAS_IMAGER
 #define TEKK_HAS_IMAGER 0
#endif
#ifndef TEKK_HAS_LIMITER
 #define TEKK_HAS_LIMITER 0
#endif

// The plugin's display name. JUCE defines JucePlugin_Name per target from the
// juce_add_plugin PRODUCT_NAME, so each module plugin names itself; the headless
// test / measurement apps (no plugin defines) fall back to the combined name.
#ifndef TEKK_PLUGIN_NAME
 #ifdef JucePlugin_Name
  #define TEKK_PLUGIN_NAME JucePlugin_Name
 #else
  #define TEKK_PLUGIN_NAME "TekkChild TC-670 Vari-Mu Compressor"
 #endif
#endif
