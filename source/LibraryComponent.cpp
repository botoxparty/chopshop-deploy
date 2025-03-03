/*
  ==============================================================================

    LibraryComponent.cpp
    Created: 17 Jan 2025 11:02:15pm
    Author:  Adam Hammad

  ==============================================================================
*/

#include "LibraryComponent.h"
#include "minibpm.h"

namespace te = tracktion::engine;

LibraryComponent::LibraryComponent(te::Engine& engineToUse)
    : engine(engineToUse)
{
    // Create or load the library project
    auto projectFile = juce::File::getSpecialLocation(juce::File::userMusicDirectory)
                        .getChildFile("ChopShop").getChildFile("Library.tracktion");
    
    // Create the directory if it doesn't exist
    auto projectDir = juce::File::getSpecialLocation(juce::File::userMusicDirectory).getChildFile("ChopShop");
    bool dirCreated = projectDir.createDirectory();
    DBG("Project directory creation result: " + juce::String(dirCreated ? "Success" : "Failed") + 
        " Path: " + projectDir.getFullPathName());
    
    // Get or create the library project
    libraryProject = engine.getProjectManager().getProject(projectFile);
    if (libraryProject == nullptr || !libraryProject->isValid())
    {
        DBG("Attempting to create new project at: " + projectFile.getFullPathName());
        libraryProject = engine.getProjectManager().createNewProject(projectFile);
        if (libraryProject != nullptr)
        {
            libraryProject->createNewProjectId();
            libraryProject->setName("ChopShop Library");
            libraryProject->setDescription("Created: " + juce::Time::getCurrentTime().toString(true, false));
            
            if (libraryProject->save())
            {
                DBG("Created and saved new ChopShop Library project at: " + projectFile.getFullPathName());
            }
            else
            {
                DBG("Failed to save project!");
            }
        }
        else
        {
            DBG("Failed to create new project!");
        }
    }
    else
    {
        DBG("Loaded existing ChopShop Library project from: " + projectFile.getFullPathName());
        DBG("Project contains " + juce::String(libraryProject->getNumProjectItems()) + " items");
        
        // Log the items in the project
        for (int i = 0; i < libraryProject->getNumProjectItems(); ++i)
        {
            auto item = libraryProject->getProjectItemAt(i);
            if (item != nullptr)
            {
                DBG("  Item " + juce::String(i) + ": " + item->getName() + 
                    " (BPM: " + juce::String(item->getNamedProperty("bpm").getFloatValue(), 1) + 
                    ", File: " + item->getSourceFile().getFileName() + ")");
            }
        }
    }
    
    // Set up add file button
    addFileButton.setColour(juce::TextButton::buttonColourId, black);
    addFileButton.setColour(juce::TextButton::textColourOffId, matrixGreen);
    addFileButton.setColour(juce::TextButton::textColourOnId, matrixGreen);
    addAndMakeVisible(addFileButton);
    
    // Set up remove file button
    removeFileButton.setColour(juce::TextButton::buttonColourId, black);
    removeFileButton.setColour(juce::TextButton::textColourOffId, matrixGreen);
    removeFileButton.setColour(juce::TextButton::textColourOnId, matrixGreen);
    addAndMakeVisible(removeFileButton);
    
    // Set up edit BPM button
    editBpmButton.setButtonText("Edit BPM");
    editBpmButton.setColour(juce::TextButton::buttonColourId, black);
    editBpmButton.setColour(juce::TextButton::textColourOffId, matrixGreen);
    editBpmButton.setColour(juce::TextButton::textColourOnId, matrixGreen);
    addAndMakeVisible(editBpmButton);
    
    // Set up playlist table
    playlistTable = std::make_unique<juce::TableListBox>();
    playlistTable->setModel(this);
    playlistTable->getHeader().addColumn("Name", 1, 300);
    playlistTable->getHeader().addColumn("BPM", 2, 100);
    playlistTable->getHeader().setStretchToFitActive(true);
    playlistTable->setColour(juce::ListBox::backgroundColourId, black);
    playlistTable->setColour(juce::ListBox::outlineColourId, matrixGreen.withAlpha(0.5f));
    playlistTable->setColour(juce::ListBox::textColourId, matrixGreen);
    addAndMakeVisible(playlistTable.get());
    
    // Enable sorting
    playlistTable->getHeader().setSortColumnId(1, true); // Default sort by name
    
    // Set up button callbacks
    addFileButton.onClick = [this]() {
        fileChooser = std::make_shared<juce::FileChooser>(
            "Select Audio Files",
            juce::File::getSpecialLocation(juce::File::userMusicDirectory),
            "*.wav;*.mp3;*.aif;*.aiff");
            
        fileChooser->launchAsync(juce::FileBrowserComponent::openMode | 
                           juce::FileBrowserComponent::canSelectFiles |
                           juce::FileBrowserComponent::canSelectMultipleItems,
                           [this](const juce::FileChooser& fc) {
                               auto results = fc.getResults();
                               for (const auto& file : results) {
                                   if (file.exists()) {
                                       addToLibrary(file);
                                   }
                               }
                           });
    };
    
    removeFileButton.onClick = [this]() {
        auto selectedRow = playlistTable->getSelectedRow();
        if (selectedRow >= 0) {
            removeFromLibrary(selectedRow);
        }
    };
    
    editBpmButton.onClick = [this]() {
        auto selectedRow = playlistTable->getSelectedRow();
        if (selectedRow >= 0) {
            showBpmEditorWindow(selectedRow);
        }
    };
    
    // Load existing library
    loadLibrary();
}

LibraryComponent::~LibraryComponent()
{
    // No need to explicitly save as the Project class handles this
}

void LibraryComponent::paint(juce::Graphics& g)
{
    g.fillAll(black);
    g.setColour(matrixGreen.withAlpha(0.5f));
    g.drawRect(getLocalBounds(), 1);
}

void LibraryComponent::resized()
{
    auto bounds = getLocalBounds();
    auto buttonHeight = 30;
    
    // Playlist table takes all space except bottom button area
    auto buttonArea = bounds.removeFromBottom(buttonHeight);
    playlistTable->setBounds(bounds.reduced(2));
    
    // Add buttons at the bottom
    addFileButton.setBounds(buttonArea.removeFromLeft(100).reduced(2));
    removeFileButton.setBounds(buttonArea.removeFromLeft(100).reduced(2));
    editBpmButton.setBounds(buttonArea.removeFromLeft(100).reduced(2));
}

// TableListBoxModel implementations
int LibraryComponent::getNumRows()
{
    return libraryProject ? libraryProject->getNumProjectItems() : 0;
}

void LibraryComponent::paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected)
{
    if (rowIsSelected)
        g.fillAll(matrixGreen.withAlpha(0.3f));
}

void LibraryComponent::paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected)
{
    if (!libraryProject || rowNumber >= libraryProject->getNumProjectItems())
        return;
        
    auto projectItem = libraryProject->getProjectItemAt(rowNumber);
    if (projectItem == nullptr)
        return;
        
    g.setColour(matrixGreen);
    
    if (columnId == 1) // Name column
        g.drawText(projectItem->getName(), 2, 0, width - 4, height, juce::Justification::centredLeft);
    else if (columnId == 2) // BPM column
        g.drawText(juce::String(projectItem->getNamedProperty("bpm").getFloatValue(), 1), 2, 0, width - 4, height, juce::Justification::centred);
}

void LibraryComponent::cellDoubleClicked(int rowNumber, int columnId, const juce::MouseEvent&)
{
    if (!libraryProject || rowNumber >= libraryProject->getNumProjectItems())
        return;
        
    auto projectItem = libraryProject->getProjectItemAt(rowNumber);
    if (projectItem != nullptr && onFileSelected) {
        juce::File file(projectItem->getSourceFile());
        if (file.exists())
            onFileSelected(file);
    }
}

void LibraryComponent::cellClicked(int rowNumber, int columnId, const juce::MouseEvent& event)
{
    if (event.mods.isRightButtonDown() && libraryProject && rowNumber < libraryProject->getNumProjectItems())
    {
        auto projectItem = libraryProject->getProjectItemAt(rowNumber);
        if (projectItem == nullptr)
            return;
            
        juce::PopupMenu menu;
        menu.addItem(1, "Show in Finder");
        menu.addItem(2, "Remove");

        menu.showMenuAsync(juce::PopupMenu::Options(), [this, rowNumber, projectItem](int result)
        {
            if (result == 1) // Show in Finder
            {
                juce::File file(projectItem->getSourceFile());
                if (file.exists())
                    file.revealToUser();
            }
            else if (result == 2) // Remove
            {
                removeFromLibrary(rowNumber);
            }
        });
    }
}

void LibraryComponent::sortOrderChanged(int newSortColumnId, bool isForwards)
{
    if (newSortColumnId != sortedColumnId || isForwards != sortedForward)
    {
        sortedColumnId = newSortColumnId;
        sortedForward = isForwards;
        
        // We can't directly sort the project items, so we'll need to reload the table
        // after sorting is changed
        playlistTable->updateContent();
    }
}

void LibraryComponent::addToLibrary(const juce::File& file)
{
    // Log the file we're trying to add
    DBG("Attempting to add file to library: " + file.getFullPathName());
    
    if (!libraryProject)
    {
        DBG("ERROR: No library project available");
        return;
    }
    
    if (!file.existsAsFile())
    {
        DBG("ERROR: File does not exist: " + file.getFullPathName());
        return;
    }
    
    // Check if the project is valid and not read-only
    if (!libraryProject->isValid())
    {
        DBG("ERROR: Library project is not valid");
        return;
    }
    
    if (libraryProject->isReadOnly())
    {
        DBG("ERROR: Library project is read-only");
        return;
    }
    
    // Check if the file format is supported
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    
    if (formatManager.findFormatForFileExtension(file.getFileExtension()) == nullptr)
    {
        DBG("ERROR: Unsupported file format: " + file.getFileExtension());
        
        // Check if MP3 support is enabled
        bool mp3Supported = false;
        for (int i = 0; i < formatManager.getNumKnownFormats(); ++i)
        {
            auto* format = formatManager.getKnownFormat(i);
            if (format->getFormatName().containsIgnoreCase("MP3"))
            {
                mp3Supported = true;
                break;
            }
        }
        
        if (!mp3Supported && file.getFileExtension().equalsIgnoreCase(".mp3"))
        {
            DBG("ERROR: MP3 support is not enabled in this build");
        }
        
        return;
    }
    
    // Calculate BPM
    float detectedBPM = 120.0f; // Default BPM
    
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (reader)
    {
        DBG("Successfully created audio reader for file: " + file.getFileName() + 
            " (Sample rate: " + juce::String(reader->sampleRate) + 
            ", Channels: " + juce::String(reader->numChannels) + 
            ", Length: " + juce::String(reader->lengthInSamples) + " samples)");
            
        // Create MiniBPM detector
        breakfastquay::MiniBPM bpmDetector(reader->sampleRate);
        bpmDetector.setBPMRange(60, 180);  // typical range for music
        
        // Process audio in chunks
        const int blockSize = 1024;
        juce::AudioBuffer<float> buffer(1, blockSize);
        std::vector<float> samples(blockSize);
        
        for (int pos = 0; pos < reader->lengthInSamples; pos += blockSize) 
        {
            const int numSamples = std::min(blockSize, 
                static_cast<int>(reader->lengthInSamples - pos));
                
            reader->read(&buffer, 0, numSamples, pos, true, false);
            memcpy(samples.data(), buffer.getReadPointer(0), numSamples * sizeof(float));
            
            bpmDetector.process(samples.data(), numSamples);
        }
        
        float tempBPM = bpmDetector.estimateTempo();
        if (tempBPM > 0)
        {
            detectedBPM = tempBPM;
            DBG("BPM detection successful: " + juce::String(detectedBPM, 1));
        }
        else
        {
            DBG("BPM detection failed, using default BPM: " + juce::String(detectedBPM, 1));
        }
    }
    else
    {
        DBG("ERROR: Failed to create audio reader for file: " + file.getFileName());
        return; // If we can't read the file, we shouldn't try to add it
    }
    
    // Check if the file is already in the library
    auto existingItem = libraryProject->getProjectItemForFile(file);
    if (existingItem != nullptr)
    {
        DBG("File already exists in library: " + file.getFileName() + 
            " (ID: " + existingItem->getID().toString() + ")");
            
        // Update the BPM if needed
        float existingBPM = existingItem->getNamedProperty("bpm").getFloatValue();
        if (std::abs(existingBPM - detectedBPM) > 0.1f)
        {
            DBG("Updating BPM from " + juce::String(existingBPM, 1) + 
                " to " + juce::String(detectedBPM, 1));
                
            existingItem->setNamedProperty("bpm", juce::String(detectedBPM));
            libraryProject->save();
            playlistTable->updateContent();
        }
        return;
    }
    
    // Log the file type we're creating
    juce::String fileType = file.hasFileExtension("mid;midi") ? 
        te::ProjectItem::midiItemType() : te::ProjectItem::waveItemType();
    DBG("Creating project item with type: " + fileType);
    
    // Create a new project item for the file
    try
    {
        auto projectItem = libraryProject->createNewItem(
            file,                                   // File to reference
            fileType,                               // Type
            file.getFileNameWithoutExtension(),     // Name
            "",                                     // Description
            te::ProjectItem::Category::imported,    // Category
            true);                                  // Add at top of list
        
        // Store the BPM as a named property
        if (projectItem != nullptr) 
        {
            DBG("Successfully created project item: " + projectItem->getID().toString());
            
            // Set the BPM property
            projectItem->setNamedProperty("bpm", juce::String(detectedBPM));
            
            // Save the project
            DBG("Saving project...");
            libraryProject->save();
            
            // Update the table
            playlistTable->updateContent();
            
            DBG("Added file to library: " + file.getFileName() + 
                " (BPM: " + juce::String(detectedBPM, 1) + 
                ", ID: " + projectItem->getID().toString() + ")");
            DBG("Library now contains " + juce::String(libraryProject->getNumProjectItems()) + " items");
        }
        else 
        {
            DBG("ERROR: createNewItem returned nullptr for file: " + file.getFileName());
            
            // Try to get more information about why it failed
                
            // Check if the file can be opened for reading
            std::unique_ptr<juce::FileInputStream> fileStream(file.createInputStream());
            if (fileStream == nullptr || !fileStream->openedOk())
            {
                DBG("ERROR: Cannot open file for reading");
            }
            else
            {
                DBG("File can be opened for reading");
            }
        }
    }
    catch (const std::exception& e)
    {
        DBG("EXCEPTION while adding file to library: " + juce::String(e.what()));
    }
    catch (...)
    {
        DBG("UNKNOWN EXCEPTION while adding file to library");
    }
}

void LibraryComponent::removeFromLibrary(int index)
{
    if (!libraryProject || index < 0 || index >= libraryProject->getNumProjectItems())
        return;
        
    auto projectItem = libraryProject->getProjectItemAt(index);
    auto projectItemID = libraryProject->getProjectItemID(index);
    
    if (projectItem != nullptr) {
        DBG("Removing item from library: " + projectItem->getName() + 
            " (ID: " + projectItemID.toString() + ")");
    }
    
    libraryProject->removeProjectItem(projectItemID, false); // false = don't delete source material
    libraryProject->save();
    playlistTable->updateContent();
    
    DBG("Library now contains " + juce::String(libraryProject->getNumProjectItems()) + " items");
}

void LibraryComponent::loadLibrary()
{
    // The project is already loaded in the constructor
    playlistTable->updateContent();
    
    // Log the current state of the library
    if (libraryProject) {
        DBG("Library loaded with " + juce::String(libraryProject->getNumProjectItems()) + " items");
        
        // Sort the items if needed
        if (sortedColumnId != 0) {
            DBG("Items are sorted by " + juce::String(sortedColumnId == 1 ? "Name" : "BPM") + 
                (sortedForward ? " (ascending)" : " (descending)"));
        }
    }
}

te::ProjectItem::Ptr LibraryComponent::getProjectItemForFile(const juce::File& file) const
{
    if (!libraryProject)
    {
        DBG("getProjectItemForFile: No library project available");
        return nullptr;
    }
        
    auto projectItem = libraryProject->getProjectItemForFile(file);
    
    if (projectItem != nullptr)
    {
        DBG("Found project item for file: " + file.getFileName() + 
            " (BPM: " + juce::String(projectItem->getNamedProperty("bpm").getFloatValue(), 1) + 
            ", ID: " + projectItem->getID().toString() + ")");
    }
    else
    {
        DBG("No project item found for file: " + file.getFileName());
    }
    
    return projectItem;
}

void LibraryComponent::showBpmEditorWindow(int rowIndex)
{
    DBG("Opening BPM editor for row: " + juce::String(rowIndex));
    
    if (!libraryProject)
    {
        DBG("ERROR: No library project available");
        return;
    }
    
    if (rowIndex < 0 || rowIndex >= libraryProject->getNumProjectItems())
    {
        DBG("ERROR: Invalid row index: " + juce::String(rowIndex) + 
            " (Project has " + juce::String(libraryProject->getNumProjectItems()) + " items)");
        return;
    }

    auto projectItem = libraryProject->getProjectItemAt(rowIndex);
    if (projectItem == nullptr)
    {
        DBG("ERROR: Failed to get project item at index: " + juce::String(rowIndex));
        return;
    }
    
    DBG("Editing BPM for item: " + projectItem->getName() + 
        " (ID: " + projectItem->getID().toString() + 
        ", File: " + projectItem->getSourceFile().getFileName() + ")");
        
    float currentBpm = projectItem->getNamedProperty("bpm").getFloatValue();
    if (currentBpm <= 0)
    {
        currentBpm = 120.0f;
        DBG("Invalid BPM value, using default: " + juce::String(currentBpm, 1));
    }
    else
    {
        DBG("Current BPM: " + juce::String(currentBpm, 1));
    }
    
    juce::DialogWindow::LaunchOptions options;
    
    auto content = std::make_unique<juce::Component>();
    content->setSize(200, 150);
    
    auto editor = new juce::TextEditor();
    editor->setBounds(50, 20, 100, 24);
    editor->setText(juce::String(currentBpm, 1));
    editor->setInputRestrictions(6, "0123456789.");
    editor->setColour(juce::TextEditor::backgroundColourId, black);
    editor->setColour(juce::TextEditor::textColourId, matrixGreen);
    editor->setColour(juce::TextEditor::outlineColourId, matrixGreen.withAlpha(0.5f));
    content->addAndMakeVisible(editor);
    
    auto halfButton = new juce::TextButton("1/2x");
    halfButton->setBounds(30, 60, 60, 24);
    halfButton->setColour(juce::TextButton::buttonColourId, black);
    halfButton->setColour(juce::TextButton::textColourOffId, matrixGreen);
    halfButton->setColour(juce::TextButton::textColourOnId, matrixGreen);
    halfButton->onClick = [editor]() {
        double currentValue = editor->getText().getDoubleValue();
        double newValue = currentValue * 0.5;
        editor->setText(juce::String(newValue, 1));
        DBG("BPM halved: " + juce::String(currentValue, 1) + " -> " + juce::String(newValue, 1));
    };
    content->addAndMakeVisible(halfButton);
    
    auto doubleButton = new juce::TextButton("2x");
    doubleButton->setBounds(110, 60, 60, 24);
    doubleButton->setColour(juce::TextButton::buttonColourId, black);
    doubleButton->setColour(juce::TextButton::textColourOffId, matrixGreen);
    doubleButton->setColour(juce::TextButton::textColourOnId, matrixGreen);
    doubleButton->onClick = [editor]() {
        double currentValue = editor->getText().getDoubleValue();
        double newValue = currentValue * 2.0;
        editor->setText(juce::String(newValue, 1));
        DBG("BPM doubled: " + juce::String(currentValue, 1) + " -> " + juce::String(newValue, 1));
    };
    content->addAndMakeVisible(doubleButton);
    
    auto okButton = new juce::TextButton("OK");
    okButton->setBounds(50, 100, 100, 24);
    okButton->setColour(juce::TextButton::buttonColourId, black);
    okButton->setColour(juce::TextButton::textColourOffId, matrixGreen);
    okButton->setColour(juce::TextButton::textColourOnId, matrixGreen);
    
    // Store a reference to the project item for the lambda
    te::ProjectItem::Ptr itemRef = projectItem;
    
    okButton->onClick = [this, itemRef, editor, currentBpm]() {
        float newBpm = editor->getText().getFloatValue();
        
        if (newBpm <= 0)
        {
            DBG("ERROR: Invalid BPM value entered: " + editor->getText());
            return;
        }
        
        if (itemRef != nullptr) 
        {
            DBG("Updating BPM for item: " + itemRef->getName() + 
                " from " + juce::String(currentBpm, 1) + 
                " to " + juce::String(newBpm, 1));
                
            try
            {
                itemRef->setNamedProperty("bpm", juce::String(newBpm));
                
                DBG("Saving project after BPM update...");
                libraryProject->save();
                
                playlistTable->updateContent();
                DBG("BPM updated successfully");
            }
            catch (const std::exception& e)
            {
                DBG("EXCEPTION while updating BPM: " + juce::String(e.what()));
            }
            catch (...)
            {
                DBG("UNKNOWN EXCEPTION while updating BPM");
            }
        }
        else
        {
            DBG("ERROR: Project item reference is null");
        }
        
        if (auto* dw = juce::Component::getCurrentlyModalComponent())
            dw->exitModalState(0);
    };
    content->addAndMakeVisible(okButton);
    
    content->setColour(juce::ResizableWindow::backgroundColourId, black);
    
    options.content.setOwned(content.release());
    options.dialogTitle = "Edit BPM";
    options.dialogBackgroundColour = black;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    
    DBG("Launching BPM editor dialog");
    options.launchAsync();
}

// FileBrowserListener methods (no longer used but kept for interface)
void LibraryComponent::selectionChanged() {}
void LibraryComponent::fileClicked(const juce::File& file, const juce::MouseEvent& e) {}
void LibraryComponent::fileDoubleClicked(const juce::File& file) {}
void LibraryComponent::browserRootChanged(const juce::File& newRoot) {}

