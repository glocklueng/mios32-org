/* -*- mode:C++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*- */
// $Id$
/*
 * MIOS Studio Main
 *
 * ==========================================================================
 *
 *  Copyright (C) 2010 Thorsten Klose (tk@midibox.org)
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
 * 
 * ==========================================================================
 */

#include "includes.h"
#include "gui/MiosStudio.h"

class MiosStudioWindow : public DocumentWindow
{
public:
    //==============================================================================
    MiosStudioWindow() 
        : DocumentWindow (T("MIOS Studio"),
                          Colours::lightgrey, 
                          DocumentWindow::allButtons,
                          true)
    {
        // initialise our settings file..
        ApplicationProperties::getInstance()->setStorageParameters(T("MIOS_Studio"),
                                                                   T(".xml"),
                                                                   String::empty,
                                                                   1000,
                                                                   PropertiesFile::storeAsXML);

        // Create an instance of our main content component, and add it 
        // to our window.
        MiosStudio* const contentComponent = new MiosStudio();
        setContentComponent(contentComponent, true, true);
        centreWithSize(getWidth(), getHeight());

        setVisible(true);

    }

    ~MiosStudioWindow()
    {
        // (the content component will be deleted automatically, so no need to do it here)
    }

    //==============================================================================
    void closeButtonPressed()
    {
        // When the user presses the close button, we'll tell the app to quit. This 
        // window will be deleted by our MiosStudioApplication::shutdown() method
        // 
        JUCEApplication::quit();
    }

#if 0
    //==============================================================================
    PropertySet* MiosStudioWindow::getGlobalSettings()
    {
        /* If you want this class to store the settings, you can set up an
           ApplicationProperties object and use this method as it is, or override this
           method to return your own custom PropertySet.
           
           If using this method without changing it, you'll probably need to call
           ApplicationProperties::setStorageParameters() in your plugin's constructor to
           tell it where to save the file.
        */
        return ApplicationProperties::getInstance()->getUserSettings();
    }
#endif

};

//==============================================================================
/** This is the application object that is started up when Juce starts. It handles
    the initialisation and shutdown of the whole application.
*/
class JUCEMiosStudioApplication : public JUCEApplication
{
    /* Important! NEVER embed objects directly inside your JUCEApplication class! Use
       ONLY pointers to objects, which you should create during the initialise() method
       (NOT in the constructor!) and delete in the shutdown() method (NOT in the
       destructor!)

       This is because the application object gets created before Juce has been properly
       initialised, so any embedded objects would also get constructed too soon.
   */
    MiosStudioWindow* miosStudioWindow;

public:
    //==============================================================================
    JUCEMiosStudioApplication()
        : miosStudioWindow (0)
    {
        // NEVER do anything in here that could involve any Juce function being called
        // - leave all your startup tasks until the initialise() method.
    }

    ~JUCEMiosStudioApplication()
    {
        // Your shutdown() method should already have done all the things necessary to
        // clean up this app object, so you should never need to put anything in
        // the destructor.

        // Making any Juce calls in here could be very dangerous...
    }

    //==============================================================================
    void initialise (const String& commandLine)
    {
        // create the main window...
        miosStudioWindow = new MiosStudioWindow();
        miosStudioWindow->setUsingNativeTitleBar(true);

        /*  ..and now return, which will fall into to the main event
            dispatch loop, and this will run until something calls
            JUCEAppliction::quit().

            In this case, JUCEAppliction::quit() will be called by the
            hello world window being clicked.
        */
    }

    void shutdown()
    {
        ApplicationProperties::getInstance()->closeFiles();

        // clear up..
        if( miosStudioWindow != 0 )
            delete miosStudioWindow;
    }


    //==============================================================================
    const String getApplicationName()
    {
        return T("MIOS Studio");
    }

    const String getApplicationVersion()
    {
        return T("1.0");
    }

    bool moreThanOneInstanceAllowed()
    {
        return true;
    }

    void anotherInstanceStarted (const String& commandLine)
    {
    }
};


//==============================================================================
// This macro creates the application's main() function..
START_JUCE_APPLICATION (JUCEMiosStudioApplication)
