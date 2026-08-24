#include "MainComponent.h"

#include "Config/AudioConfig.h"

MainComponent::MainComponent()
    : audioEngine(delayProcessor)
{
    setSize(760, 900);

    addAndMakeVisible(deviceTypeBox);
    addAndMakeVisible(inputDeviceBox);
    addAndMakeVisible(outputDeviceBox);

    deviceTypeBox.onChange = [this]
    {
        const auto index = deviceTypeBox.getSelectedItemIndex();
        if (juce::isPositiveAndBelow(index, deviceTypeNames.size()))
        {
            selectedDeviceType = deviceTypeNames[index];
            selectedInputDevice.clear();
            selectedOutputDevice.clear();
            refreshDeviceChoices();
        }
    };
    inputDeviceBox.onChange = [this] { selectedInputDevice = inputDeviceBox.getText(); };
    outputDeviceBox.onChange = [this] { selectedOutputDevice = outputDeviceBox.getText(); };
    refreshDeviceChoices();

    runButton.onClick = [this] { toggleAudio(); };
    addAndMakeVisible(runButton);

    latencyKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    latencyKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 90, 24);
    latencyKnob.setRange(0.0, static_cast<double>(audio_config::maximumAddedLatencyMilliseconds), 1.0);
    latencyKnob.setValue(0.0, juce::dontSendNotification);
    latencyKnob.setTextValueSuffix(" ms");
    latencyKnob.onValueChange = [this]
    {
        delayProcessor.setAddedLatencyMilliseconds(static_cast<int>(latencyKnob.getValue()));
    };
    addAndMakeVisible(latencyKnob);

    startTimerHz(10);
}

MainComponent::~MainComponent()
{
    stopTimer();
    audioEngine.shutdown();
    audioIsRunning = false;
    shutdownAudio();
}

void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    juce::ignoreUnused(samplesPerBlockExpected, sampleRate);
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    bufferToFill.clearActiveBufferRegion();
}

void MainComponent::releaseResources()
{
}

void MainComponent::paint(juce::Graphics& graphics)
{
    graphics.fillAll(juce::Colour::fromRGB(30, 33, 39));
    graphics.setColour(juce::Colours::white);
    graphics.setFont(22.0f);
    graphics.drawText("RealtimeFeedbackEngine", 24, 18, getWidth() - 48, 30,
                      juce::Justification::centred);

    const auto addedMilliseconds = delayProcessor.getAddedLatencyMilliseconds();
    const auto addedSamples = delayProcessor.getAddedLatencySamples();
    const auto inputLatency = audioEngine.getInputLatencySamples();
    const auto outputLatency = audioEngine.getOutputLatencySamples();
    const auto totalLatency = inputLatency + outputLatency + addedSamples;
    const auto sampleRate = audioEngine.getCurrentSampleRate();
    const auto totalMilliseconds = sampleRate > 0.0
        ? static_cast<double>(totalLatency) * 1000.0 / sampleRate
        : 0.0;

    graphics.setFont(16.0f);
    graphics.drawText("Added latency: " + juce::String(addedMilliseconds) + " ms (" +
                          juce::String(addedSamples) + " samples)",
                      24, 64, getWidth() - 48, 24, juce::Justification::centred);

    graphics.setColour(juce::Colours::lightgrey);
    graphics.setFont(14.0f);
    graphics.drawText("Audio device selection (applied when Run is pressed)",
                      24, 224, getWidth() - 48, 20, juce::Justification::centred);
    graphics.drawText("Host:", 24, 252, 140, 28, juce::Justification::centredRight);
    graphics.drawText("Input:", 24, 292, 140, 28, juce::Justification::centredRight);
    graphics.drawText("Output:", 24, 332, 140, 28, juce::Justification::centredRight);

    const auto drawMeterGroup = [&graphics, this] (juce::Rectangle<int> area,
                                                   const juce::String& title,
                                                   bool inputGroup,
                                                   int channelCount)
    {
        graphics.setColour(juce::Colour::fromRGB(44, 48, 56));
        graphics.fillRoundedRectangle(area.toFloat(), 8.0f);
        graphics.setColour(juce::Colours::white);
        graphics.drawText(title, area.removeFromTop(28), juce::Justification::centred);

        const auto gap = 12;
        const auto columnWidth = (area.getWidth() - gap * (channelCount + 1))
            / juce::jmax(1, channelCount);
        for (int channel = 0; channel < channelCount; ++channel)
        {
            auto column = area.removeFromLeft(columnWidth);
            column.setX(column.getX() + gap);
            column.setY(column.getY() + 8);
            column.setHeight(column.getHeight() - 16);

            const auto dbfs = inputGroup ? audioEngine.getInputLevelDbfs(channel)
                                         : audioEngine.getOutputLevelDbfs(channel);
            const auto normalized = juce::jlimit(0.0f, 1.0f, (dbfs + 60.0f) / 60.0f);
            const auto bar = column.reduced(12, 24);

            graphics.setColour(juce::Colour::fromRGB(24, 27, 32));
            graphics.fillRoundedRectangle(bar.toFloat(), 4.0f);
            auto filled = bar;
            filled.setY(bar.getBottom() - juce::roundToInt(bar.getHeight() * normalized));
            graphics.setColour(dbfs > -6.0f ? juce::Colours::red
                                            : dbfs > -18.0f ? juce::Colours::orange
                                                             : juce::Colours::limegreen);
            graphics.fillRoundedRectangle(filled.toFloat(), 4.0f);

            graphics.setColour(juce::Colours::white);
            graphics.drawText((inputGroup ? "Input " : "Output ") + juce::String(channel + 1),
                              column.removeFromTop(20), juce::Justification::centred);
            graphics.drawText(juce::String(dbfs, 1) + " dBFS",
                              column.removeFromBottom(20), juce::Justification::centred);
        }
    };

    drawMeterGroup({ 24, 570, 344, 170 }, "Input channels", true, audio_config::inputChannels);
    drawMeterGroup({ 392, 570, 344, 170 }, "Output channels", false, audio_config::outputChannels);

    auto selectedDeviceName = audioEngine.getCurrentDeviceName();
    if (! audioIsRunning)
        if (const auto* selectedDevice = deviceManager.getCurrentAudioDevice())
            selectedDeviceName = selectedDevice->getName();

    graphics.drawText("Audio device: " + selectedDeviceName,
                      24, 748, getWidth() - 48, 20, juce::Justification::centred);
    const auto displayedHost = audioIsRunning ? audioEngine.getCurrentDeviceType()
                                               : selectedDeviceType;
    graphics.drawText("Host: " + displayedHost +
                          " | " + juce::String(sampleRate, 0) + " Hz | " +
                          juce::String(audioEngine.getCurrentBlockSize()) + " samples",
                      24, 772, getWidth() - 48, 20, juce::Justification::centred);
    graphics.drawText("Actual device latency: input " + juce::String(inputLatency) +
                          " + output " + juce::String(outputLatency) + " samples",
                      24, 796, getWidth() - 48, 20, juce::Justification::centred);
    graphics.drawText("Estimated total latency: " + juce::String(totalLatency) + " samples (" +
                          juce::String(totalMilliseconds, 2) + " ms)",
                      24, 820, getWidth() - 48, 20, juce::Justification::centred);
    graphics.drawText("Status: " + audioEngine.getStatusMessage(),
                      24, 844, getWidth() - 48, 20, juce::Justification::centred);
}

void MainComponent::resized()
{
    runButton.setBounds(getWidth() - 180, 92, 140, 34);
    latencyKnob.setBounds(getWidth() / 2 - 85, 90, 170, 145);
    deviceTypeBox.setBounds(180, 252, getWidth() - 220, 28);
    inputDeviceBox.setBounds(180, 292, getWidth() - 220, 28);
    outputDeviceBox.setBounds(180, 332, getWidth() - 220, 28);
}

void MainComponent::timerCallback()
{
    if (! automaticStartAttempted)
    {
        automaticStartAttempted = true;
        toggleAudio();
    }

    repaint();
}

void MainComponent::toggleAudio()
{
    if (audioIsRunning)
    {
        audioEngine.shutdown();
        audioIsRunning = false;
        deviceTypeBox.setEnabled(true);
        inputDeviceBox.setEnabled(true);
        outputDeviceBox.setEnabled(true);
    }
    else
    {
        audioIsRunning = audioEngine.initialise(deviceManager,
                                                selectedDeviceType,
                                                selectedInputDevice,
                                                selectedOutputDevice);
        deviceTypeBox.setEnabled(! audioIsRunning);
        inputDeviceBox.setEnabled(! audioIsRunning);
        outputDeviceBox.setEnabled(! audioIsRunning);
    }

    updateRunButton();
    repaint();
}

void MainComponent::updateRunButton()
{
    runButton.setButtonText(audioIsRunning ? "Stop" : "Run");
}

void MainComponent::refreshDeviceChoices()
{
    deviceTypeNames.clear();
    deviceTypeBox.clear(juce::dontSendNotification);

    for (auto* type : deviceManager.getAvailableDeviceTypes())
    {
        type->scanForDevices();
        deviceTypeNames.add(type->getTypeName());
        deviceTypeBox.addItem(type->getTypeName(), deviceTypeNames.size());
    }

    if (selectedDeviceType.isEmpty())
    {
        for (int index = 0; index < deviceTypeNames.size(); ++index)
        {
            if (deviceTypeNames[index].equalsIgnoreCase("ASIO"))
            {
                selectedDeviceType = deviceTypeNames[index];
                break;
            }
        }
    }

    if (selectedDeviceType.isEmpty() && deviceTypeNames.size() > 0)
        selectedDeviceType = deviceManager.getCurrentAudioDeviceType();
    if (selectedDeviceType.isEmpty() && deviceTypeNames.size() > 0)
        selectedDeviceType = deviceTypeNames[0];

    const auto typeIndex = deviceTypeNames.indexOf(selectedDeviceType);
    if (typeIndex >= 0)
        deviceTypeBox.setSelectedItemIndex(typeIndex, juce::dontSendNotification);

    juce::StringArray inputNames, outputNames;
    for (auto* type : deviceManager.getAvailableDeviceTypes())
    {
        if (type->getTypeName() == selectedDeviceType)
        {
            inputNames = type->getDeviceNames(true);
            outputNames = type->getDeviceNames(false);
            break;
        }
    }

    inputDeviceBox.clear(juce::dontSendNotification);
    outputDeviceBox.clear(juce::dontSendNotification);
    for (int i = 0; i < inputNames.size(); ++i)
        inputDeviceBox.addItem(inputNames[i], i + 1);
    for (int i = 0; i < outputNames.size(); ++i)
        outputDeviceBox.addItem(outputNames[i], i + 1);

    const auto preferredDevice = [] (const juce::StringArray& names)
    {
        for (const auto& name : names)
            if (name.equalsIgnoreCase("Focusrite USB ASIO"))
                return name;
        return names.isEmpty() ? juce::String() : names[0];
    };

    if (selectedInputDevice.isEmpty() || inputNames.indexOf(selectedInputDevice) < 0)
        selectedInputDevice = preferredDevice(inputNames);
    if (selectedOutputDevice.isEmpty() || outputNames.indexOf(selectedOutputDevice) < 0)
        selectedOutputDevice = preferredDevice(outputNames);

    inputDeviceBox.setText(selectedInputDevice, juce::dontSendNotification);
    outputDeviceBox.setText(selectedOutputDevice, juce::dontSendNotification);
}
