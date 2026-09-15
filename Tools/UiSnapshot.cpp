#include "../Source/PluginEditor.h"
#include <cstdlib>
#include <iostream>

namespace
{
bool writeSnapshot (juce::AudioProcessorEditor& editor, const juce::File& output,
                    int width, int height)
{
    editor.setSize (width, height);
    const auto image = editor.createComponentSnapshot (editor.getLocalBounds(), true);
    output.getParentDirectory().createDirectory();
    output.deleteFile();
    auto stream = output.createOutputStream();
    juce::PNGImageFormat png;
    return stream != nullptr && png.writeImageToStream (image, *stream);
}
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    const auto output = argc > 1
        ? (juce::String (argv[1]).startsWithChar ('/')
            ? juce::File (juce::String (argv[1]))
            : juce::File::getCurrentWorkingDirectory().getChildFile (juce::String (argv[1])))
        : juce::File::getCurrentWorkingDirectory().getChildFile ("seoul-dsp-ui.png");
    HybridWavetableAudioProcessor processor (0x53454f55u);
    // Modes:
    //   default / argc==1            -> single snapshot of the current type
    //   --all <dir> [w] [h]          -> render all 90 types into <dir>/type-NN.png
    //   <path> [w] [h] [type]        -> single snapshot of one type
    const juce::String firstArg = argc > 1 ? juce::String (argv[1]) : juce::String();
    int width = 1120;
    int height = 760;
    if (firstArg != "--all")
    {
        if (argc > 2) width = juce::jmax (1040, std::atoi (argv[2]));
        if (argc > 3) height = juce::jmax (760, std::atoi (argv[3]));
    }
    if (firstArg == "--all")
    {
        if (argc < 3)
        {
            std::cerr << "usage: SeoulDSP_UISnapshot --all <output-dir>\n";
            return 2;
        }
        const auto directory = juce::String (argv[2]).startsWithChar ('/')
            ? juce::File (juce::String (argv[2]))
            : juce::File::getCurrentWorkingDirectory().getChildFile (juce::String (argv[2]));
        if (argc > 3) width = juce::jmax (1040, std::atoi (argv[3]));
        if (argc > 4) height = juce::jmax (760, std::atoi (argv[4]));
        directory.createDirectory();
        int failures = 0;
        std::unique_ptr<juce::AudioProcessorEditor> allEditor (processor.createEditor());
        if (allEditor == nullptr)
            return 2;
        for (int type = 0; type < seoului::typeCount(); ++type)
        {
            processor.setUiType (type);
            // The editor reads uiType at construction; push later changes in
            // so each offline snapshot renders the requested presentation.
            if (auto* typedEditor = dynamic_cast<HybridWavetableAudioProcessorEditor*> (allEditor.get()))
                typedEditor->applyUiType (type);
            const auto name = "type-" + juce::String (type).paddedLeft ('0', 2) + ".png";
            const bool ok = writeSnapshot (*allEditor, directory.getChildFile (name), width, height);
            std::cout << name << (ok ? " ok" : " FAIL") << "\n";
            failures += ok ? 0 : 1;
        }
        return failures == 0 ? 0 : 1;
    }
    const int uiType = argc > 4 ? seoului::clampType (std::atoi (argv[4])) : 0;
    processor.setUiType (uiType);
    std::cout << "uiType=" << uiType << "\n";
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    if (editor == nullptr)
        return 2;
    for (int i = 0; i < editor->getNumChildComponents(); ++i)
        if (auto* combo = dynamic_cast<juce::ComboBox*> (editor->getChildComponent (i)))
        {
            std::cout << "combo[" << i << "] text=\"" << combo->getText().toStdString()
                      << "\" index=" << combo->getSelectedItemIndex()
                      << " bounds=" << combo->getBounds().toString().toStdString() << "\n";
            for (int child = 0; child < combo->getNumChildComponents(); ++child)
                if (auto* label = dynamic_cast<juce::Label*> (combo->getChildComponent (child)))
                    std::cout << "  label visible=" << label->isVisible()
                              << " textColour=" << label->findColour (juce::Label::textColourId).toString().toStdString()
                              << " bounds=" << label->getBounds().toString().toStdString() << "\n";
        }
    return writeSnapshot (*editor, output, width, height) ? 0 : 1;
}
