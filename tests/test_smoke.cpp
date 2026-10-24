#include <catch2/catch_test_macros.hpp>

#include "PluginProcessor.h"

TEST_CASE("processor is a MIDI effect")
{
    swingers::SwingersProcessor p;
    CHECK(p.isMidiEffect());
    CHECK(p.acceptsMidi());
    CHECK(p.producesMidi());
}

TEST_CASE("every program has a name (the VST3 validator rejects unnamed programs)")
{
    swingers::SwingersProcessor p;
    REQUIRE(p.getNumPrograms() >= 1);
    for (int i = 0; i < p.getNumPrograms(); ++i)
        CHECK(p.getProgramName(i).isNotEmpty());
}
