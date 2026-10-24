#include <catch2/catch_test_macros.hpp>

#include "PluginProcessor.h"

TEST_CASE("processor is a MIDI effect")
{
    swingers::SwingersProcessor p;
    CHECK(p.isMidiEffect());
    CHECK(p.acceptsMidi());
    CHECK(p.producesMidi());
}
