#include "../../Source/DSP/UnisonBank.h"
#include "TestHarness.h"

#include <cmath>

int main()
{
    audioquality::TestHarness test;
    for (const auto count : { 1, 2, 4, 8 })
    {
        const auto lanes = makeUnisonLayout (count, 12.0f, 0.8f);
        float centsMean = 0.0f;
        for (int i = 0; i < count; ++i)
        {
            centsMean += lanes[(size_t) i].cents;
            test.expect (lanes[(size_t) i].pan >= -1.0f && lanes[(size_t) i].pan <= 1.0f,
                         "unison pan remains in range");
            test.expect (std::isfinite (lanes[(size_t) i].gain), "unison gain is finite");
            test.expect (std::abs (lanes[(size_t) i].gain - 1.0f / std::sqrt ((float) count)) < 1.0e-6f,
                         "unison normalization is 1/sqrt(count)");
        }
        test.expect (std::abs (centsMean / (float) count) <= 0.2f, "unison mean detune is centered");
        for (int i = 0; i < count / 2; ++i)
        {
            test.expect (std::abs (lanes[(size_t) i].cents + lanes[(size_t) (count - 1 - i)].cents) < 1.0e-5f,
                         "paired cents offsets sum to zero");
            test.expect (std::abs (lanes[(size_t) i].pan + lanes[(size_t) (count - 1 - i)].pan) < 1.0e-5f,
                         "paired pan positions sum to zero");
        }
    }
    return test.result();
}
