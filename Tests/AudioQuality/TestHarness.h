#pragma once

#include <JuceHeader.h>
#include <iostream>

namespace audioquality
{
class TestHarness
{
public:
    void expect (bool condition, const juce::String& message)
    {
        if (! condition)
        {
            ++failures;
            std::cerr << "FAIL: " << message << "\n";
        }
    }

    int failureCount() const noexcept { return failures; }
    int result() const noexcept { return failures == 0 ? 0 : 1; }

private:
    int failures = 0;
};
}
