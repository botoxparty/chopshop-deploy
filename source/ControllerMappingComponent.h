#pragma once

#include <JuceHeader.h>
#include "GamepadManager.h"

class ControllerMappingComponent : public juce::Component,
                                  private juce::Timer
{
public:
    ControllerMappingComponent();
    ~ControllerMappingComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    
    // Add timer callback
    void timerCallback() override;

    struct ControllerMapping
    {
        int buttonId;
        juce::String actionName;
        bool isAxis;
    };

    void drawMappingsList(juce::Graphics& g, juce::Rectangle<float> bounds);

private:
    friend class ComponentListener;
    
    class ComponentListener : public juce::ComponentListener
    {
    public:
        ComponentListener(ControllerMappingComponent& owner) : owner_(owner) {}
        
        void componentBeingDeleted(juce::Component& component) override
        {
            owner_.mappingDialog = nullptr;
            delete this;
        }
        
    private:
        ControllerMappingComponent& owner_;
    };

    void drawButton(juce::Graphics& g, juce::Point<float> center, float radius, 
                   const juce::String& label, bool isHighlighted);
    
    juce::TextButton mappingButton {"Game Controller"};
    juce::DialogWindow* mappingDialog = nullptr;
    
    std::vector<ControllerMapping> mappings;
    
    // Store button positions for hit detection
    struct ButtonPosition
    {
        juce::Point<float> center;
        float radius;
        juce::String name;
        int buttonId;
    };
    
    std::vector<ButtonPosition> buttonPositions;
    
    void drawButton(juce::Graphics& g, 
                    juce::Point<float> center,
                    float radius,
                    const juce::String& symbol,
                    int buttonId,
                    const juce::String& mapping);
                    
    juce::Colour getButtonColor(int buttonId);
    
    void drawTouchpad(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawAnalogSticks(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawDpad(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawTriggers(juce::Graphics& g, juce::Rectangle<float> bounds);
    
    bool isControllerConnected = false;
    juce::String connectedControllerName;
    
    void checkControllerConnection();
    
    void updateButtonAppearance();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ControllerMappingComponent)
}; 