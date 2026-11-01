// Rend l'éditeur hors écran en PNG (3 pages) : s90_snapshot <dossier>
#include "../Source/PluginEditor.h"
using namespace juce;
int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    const File dir (argc > 1 ? argv[1] : "/tmp");
    S90Processor p;
    p.setPlayConfigDetails (0, 2, 44100.0, 256);
    p.prepareToPlay (44100.0, 256);
    for (int mode = 0; mode < 3; ++mode)
    {
        p.setParam ("mode", (float) mode);
        if (mode == 1) p.setParam ("scene", 40.f);
        MessageManager::getInstance()->runDispatchLoopUntil (100);
        std::unique_ptr<AudioProcessorEditor> ed (p.createEditor());
        ed->setSize (1100, 700);
        MessageManager::getInstance()->runDispatchLoopUntil (150);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        FileOutputStream os (dir.getChildFile ("page" + String (mode) + ".png"));
        os.setPosition (0); os.truncate();
        PNGImageFormat().writeImageToStream (img, os);
    }
    return 0;
}
