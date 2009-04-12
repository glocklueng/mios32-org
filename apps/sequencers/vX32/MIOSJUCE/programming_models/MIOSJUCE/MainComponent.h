/*
  ==============================================================================

  This is an automatically generated file created by the Jucer!

  Creation date:  12 Apr 2009 10:44:35 pm

  Be careful when adding custom code to these files, as only the code within
  the "//[xyz]" and "//[/xyz]" sections will be retained when the file is loaded
  and re-saved.

  Jucer version: 1.11

  ------------------------------------------------------------------------------

  The Jucer is part of the JUCE library - "Jules' Utility Class Extensions"
  Copyright 2004-6 by Raw Material Software ltd.

  ==============================================================================
*/

#ifndef __JUCER_HEADER_MAINCOMPONENT_MAINCOMPONENT_286B3BD0__
#define __JUCER_HEADER_MAINCOMPONENT_MAINCOMPONENT_286B3BD0__

//[Headers]     -- You can add your own extra header files here --
#include "MIDIDialog.h"
//[/Headers]

#include <AppComponent.h>


//==============================================================================
/**
                                                                    //[Comments]
    An auto-generated component, created by the Jucer.

    Describe your class and how it works here!
                                                                    //[/Comments]
*/
class MainComponent  : public Component,
                       public ButtonListener
{
public:
    //==============================================================================
    MainComponent ();
    ~MainComponent();

    //==============================================================================
    //[UserMethods]     -- You can add your own custom methods in this section.

    // this wraps the midi devices
    AudioDeviceManager midiDeviceManager;

    AudioDeviceManager *GetMIDIManager()
    {
        return &midiDeviceManager;
    }

    void mouseMove(const MouseEvent &e);


    //[/UserMethods]

    void paint (Graphics& g);
    void resized();
    void buttonClicked (Button* buttonThatWasClicked);
    void parentSizeChanged();


    //==============================================================================
    juce_UseDebuggingNewOperator

private:
    //[UserVariables]   -- You can add your own custom variables in this section.
    //[/UserVariables]

    //==============================================================================
    AppComponent* UserApp;
    Label* MIOSJUCELabel;
    Label* credits;
    TextButton* quitButton;
    TextButton* ioButton;

    //==============================================================================
    // (prevent copy constructor and operator= being generated..)
    MainComponent (const MainComponent&);
    const MainComponent& operator= (const MainComponent&);
};


#endif   // __JUCER_HEADER_MAINCOMPONENT_MAINCOMPONENT_286B3BD0__
