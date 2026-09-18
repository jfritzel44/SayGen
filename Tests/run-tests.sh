#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
# Build the matching standalone configuration first to provide the JUCE archive.
TASK_TEST_CONFIG=${1:-Debug}
case "$TASK_TEST_CONFIG" in
    Debug) TASK_TEST_DEFINE=-DDEBUG=1 ;;
    Release) TASK_TEST_DEFINE=-DNDEBUG=1 ;;
    *) echo "Use Debug or Release" >&2; exit 1 ;;
esac
# These binaries are temporary; no test writes presets or launches the synth.
TASK_TEST_DIR=$(mktemp -d /tmp/syngen-tests.XXXXXX)
clang++ -std=c++17 -O2 -Wall -Wextra -pedantic Tests/EnhancedDSPTests.cpp -o "$TASK_TEST_DIR/dsp-tests"
"$TASK_TEST_DIR/dsp-tests"
clang++ -std=c++17 -O2 -Wall -Wextra Tests/OscillatorMotionTests.cpp -o "$TASK_TEST_DIR/motion-tests"
"$TASK_TEST_DIR/motion-tests"
clang++ -std=c++17 -O2 -Wall -Wextra -pedantic Tests/EffectsTests.cpp -o "$TASK_TEST_DIR/effects-tests"
"$TASK_TEST_DIR/effects-tests"
for TASK_TEST_SUITE in VoiceEngine AISoundDesigner; do
clang++ -std=c++17 -O2 "$TASK_TEST_DEFINE" -DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 \
    -DJUCE_STANDALONE_APPLICATION=1 -DJUCE_USE_CURL=0 \
    -include JuceLibraryCode/JucePluginDefines.h \
    -I JuceLibraryCode -I ../JUCE/modules "Tests/${TASK_TEST_SUITE}Tests.cpp" \
    "Builds/MacOSX/build/$TASK_TEST_CONFIG/libSynGen1.a" \
    -framework Accelerate -framework AudioToolbox -framework Cocoa -framework CoreAudio \
    -framework CoreAudioKit -framework CoreMIDI -framework DiscRecording -framework Foundation \
    -framework IOKit -framework QuartzCore -framework Security -framework WebKit \
    -framework Metal -framework MetalKit -o "$TASK_TEST_DIR/$TASK_TEST_SUITE-tests"
if [ "$TASK_TEST_SUITE" = VoiceEngine ] && [ "$#" -ge 2 ]; then
    "$TASK_TEST_DIR/$TASK_TEST_SUITE-tests" "$2"
else
    "$TASK_TEST_DIR/$TASK_TEST_SUITE-tests"
fi
done
