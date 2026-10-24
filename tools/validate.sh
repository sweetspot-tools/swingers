#!/usr/bin/env bash
# Runs the format validators on a built Swingers.
#
# Usage: tools/validate.sh <artefacts-dir>
#   <artefacts-dir> is the JUCE artefacts folder of one configuration, with one sub-folder
#   per format (VST3/Swingers.vst3, CLAP/Swingers.clap, AU/Swingers.component), e.g.
#   /tmp/swingers-build/Swingers_artefacts/Release. Bundles placed directly in it also work.
#
# - VST3: Steinberg's validator, if VST3_VALIDATOR points at the binary.
# - CLAP: clap-validator, if it is in the PATH.
# - AU:   auval, on macOS only (the component must be installed for auval to find it).
#
# A missing validator or bundle is reported and skipped. The exit code is non-zero
# if at least one validator that did run reported a failure.

set -u

if [ $# -ne 1 ] || [ ! -d "$1" ]; then
    echo "usage: $0 <artefacts-dir>" >&2
    exit 2
fi

artefacts="$1"
failed=0
ran=0

# Prints the path of a bundle, looking in <artefacts-dir>/<format>/ first, then in <artefacts-dir>.
bundle() {
    local format="$1" name="$2"
    if [ -e "$artefacts/$format/$name" ]; then
        echo "$artefacts/$format/$name"
    elif [ -e "$artefacts/$name" ]; then
        echo "$artefacts/$name"
    fi
}

run() {
    local label="$1"
    shift
    echo "== $label: $*"
    if "$@"; then
        echo "-- $label: PASS"
    else
        echo "-- $label: FAIL" >&2
        failed=1
    fi
    ran=$((ran + 1))
}

# VST3
vst3="$(bundle VST3 Swingers.vst3)"
if [ -z "${VST3_VALIDATOR:-}" ]; then
    echo "-- VST3: skipped (set VST3_VALIDATOR to Steinberg's validator binary)"
elif [ ! -x "$VST3_VALIDATOR" ]; then
    echo "-- VST3: skipped (VST3_VALIDATOR is not an executable file: $VST3_VALIDATOR)"
elif [ -z "$vst3" ]; then
    echo "-- VST3: skipped (no Swingers.vst3 under $artefacts)"
else
    run "VST3" "$VST3_VALIDATOR" "$vst3"
fi

# CLAP
clap="$(bundle CLAP Swingers.clap)"
if ! command -v clap-validator >/dev/null 2>&1; then
    echo "-- CLAP: skipped (clap-validator not found in PATH)"
elif [ -z "$clap" ]; then
    echo "-- CLAP: skipped (no Swingers.clap under $artefacts)"
else
    run "CLAP" clap-validator validate "$clap"
fi

# AU
if [ "$(uname -s)" != "Darwin" ]; then
    echo "-- AU: skipped (macOS only)"
elif ! command -v auval >/dev/null 2>&1; then
    echo "-- AU: skipped (auval not found)"
elif [ -z "$(bundle AU Swingers.component)" ]; then
    echo "-- AU: skipped (no Swingers.component under $artefacts)"
else
    run "AU" auval -v aumi Swng Swtl
fi

if [ "$ran" -eq 0 ]; then
    echo "No validator ran." >&2
fi
exit "$failed"
