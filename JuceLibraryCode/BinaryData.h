/* =========================================================================================

   This is an auto-generated file: Any edits you make may be overwritten!

*/

#pragma once

namespace BinaryData
{
    extern const char*   osc_png;
    const int            osc_pngSize = 1539462;

    extern const char*   led_off_png;
    const int            led_off_pngSize = 936212;

    extern const char*   led_on_png;
    const int            led_on_pngSize = 730756;

    extern const char*   logo_png;
    const int            logo_pngSize = 130012;

    extern const char*   toggle_on_png;
    const int            toggle_on_pngSize = 2175185;

    extern const char*   toggle_off_png;
    const int            toggle_off_pngSize = 2154322;

    extern const char*   osc_up_png;
    const int            osc_up_pngSize = 546331;

    extern const char*   osc_down_png;
    const int            osc_down_pngSize = 552902;

    extern const char*   osc_left_png;
    const int            osc_left_pngSize = 556805;

    extern const char*   osc_right_png;
    const int            osc_right_pngSize = 555084;

    extern const char*   EurostileExtendedBlack_ttf;
    const int            EurostileExtendedBlack_ttfSize = 32692;

    extern const char*   BarlowCondensedMedium_ttf;
    const int            BarlowCondensedMedium_ttfSize = 97960;

    // Number of elements in the namedResourceList and originalFileNames arrays.
    const int namedResourceListSize = 12;

    // Points to the start of a list of resource names.
    extern const char* namedResourceList[];

    // Points to the start of a list of resource filenames.
    extern const char* originalFilenames[];

    // If you provide the name of one of the binary resource variables above, this function will
    // return the corresponding data and its size (or a null pointer if the name isn't found).
    const char* getNamedResource (const char* resourceNameUTF8, int& dataSizeInBytes);

    // If you provide the name of one of the binary resource variables above, this function will
    // return the corresponding original, non-mangled filename (or a null pointer if the name isn't found).
    const char* getNamedResourceOriginalFilename (const char* resourceNameUTF8);
}
