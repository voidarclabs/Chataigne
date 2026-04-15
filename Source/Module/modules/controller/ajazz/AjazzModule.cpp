/*
  ==============================================================================

    AjazzModule.cpp
    Created: 7 Apr 2026
    Author:  Andrea Bergamasco [@vjandrea]

  ==============================================================================
*/

#include "Module/ModuleIncludes.h"
#include "AjazzModule.h"
#include "commands/AjazzCommand.h"

AjazzModule::AjazzModule(const String& name) :
	Module(name),
	isConnected(nullptr),
	device(nullptr),
	numRows(3),
	numColumns(5),
	numSideKeys(3),
	readyToSend(false),
	colorsCC("Colors"),
	imagesCC("Images"),
	textsCC("Texts"),
    sideCC("Side Icons")
{
	setupIOConfiguration(true, true);

	isConnected = moduleParams.addBoolParameter("Is Connected", "Is an Ajazz AKP connected ?", false);
	isConnected->setControllableFeedbackOnly(true);
	connectionFeedbackRef = isConnected;

	deviceType = moduleParams.addEnumParameter("Device type", "Type of the device");
	deviceType->addOption("Ajazz AKP153 (15 keys, 3 side displays)", AKP153);
	deviceType->addOption("Ajazz AKP03 (6 keys, 3 knobs/screens)", AKP03);
	deviceType->addOption("Ajazz AKP05 (10 keys, 4 knobs/screens)", AKP05);

	targetDevice = moduleParams.addEnumParameter("Target Device", "Select the physical device to control");
	targetDevice->addOption("None", "none");

    deviceWarning = moduleParams.addStringParameter("Note", "", "Knob rotation is not yet supported on this model.");
    deviceWarning->isControllableFeedbackOnly = true;
    deviceWarning->hideInEditor = true; // Hidden by default (AKP153 has no knobs)

	colorsCC.saveAndLoadRecursiveData = true;
	imagesCC.saveAndLoadRecursiveData = true;
	textsCC.saveAndLoadRecursiveData = true;
    sideCC.saveAndLoadRecursiveData = true;

	moduleParams.addChildControllableContainer(&colorsCC);
	moduleParams.addChildControllableContainer(&imagesCC);
	moduleParams.addChildControllableContainer(&textsCC);
    moduleParams.addChildControllableContainer(&sideCC);
	colorsCC.editorIsCollapsed = true;
	imagesCC.editorIsCollapsed = true;
	textsCC.editorIsCollapsed = true;
    sideCC.editorIsCollapsed = true;

	brightness = moduleParams.addIntParameter("Brightness", "Sets the brightness of the deck's backlight", 75, 0, 100);
	textSize = moduleParams.addIntParameter("Text size", "Sets the size of the text on the buttons", 10, 1, 50);

	reset = moduleParams.addTrigger("Reset", "Resets the Ajazz AKP");
	colorizeImages = moduleParams.addBoolParameter("Colorize images", "If checked, this will use both colors and images to set buttons", false);
	highlightPressedButtons = moduleParams.addBoolParameter("Highlight pressed buttons", "If checked, this will automatically highlight the buttons when pressed", true);

	defManager->add(CommandDefinition::createDef(this, "", "Set Color", &AjazzCommand::create)->addParam("action", AjazzCommand::SET_COLOR));
	defManager->add(CommandDefinition::createDef(this, "", "Set Image", &AjazzCommand::create)->addParam("action", AjazzCommand::SET_IMAGE));
	defManager->add(CommandDefinition::createDef(this, "", "Set Text", &AjazzCommand::create)->addParam("action", AjazzCommand::SET_TEXT));
	defManager->add(CommandDefinition::createDef(this, "", "Set All Color", &AjazzCommand::create)->addParam("action", AjazzCommand::SET_ALL_COLOR));
	defManager->add(CommandDefinition::createDef(this, "", "Clear Texts", &AjazzCommand::create)->addParam("action", AjazzCommand::CLEAR_TEXTS));
	defManager->add(CommandDefinition::createDef(this, "", "Set Brightness", &AjazzCommand::create)->addParam("action", AjazzCommand::SET_BRIGHTNESS));
    
    defManager->add(CommandDefinition::createDef(this, "Side", "Set Side Color", &AjazzCommand::create)->addParam("action", AjazzCommand::SET_SIDE_COLOR));
    defManager->add(CommandDefinition::createDef(this, "Side", "Set Side Image", &AjazzCommand::create)->addParam("action", AjazzCommand::SET_SIDE_IMAGE));
    defManager->add(CommandDefinition::createDef(this, "Side", "Set Side Text", &AjazzCommand::create)->addParam("action", AjazzCommand::SET_SIDE_TEXT));

	rebuildValues();
	updateDeviceList();

	AjazzManager::getInstance()->addAjazzManagerListener(this);
}

AjazzModule::~AjazzModule()
{
	if (AjazzManager::getInstanceWithoutCreating() != nullptr) AjazzManager::getInstance()->removeAjazzManagerListener(this);
	setDevice(nullptr);
}

void AjazzModule::updateDeviceList()
{
    String currentSerial = targetDevice->getValueData().toString();
    targetDevice->clearOptions();
    targetDevice->addOption("None", "none");
    
    for (auto d : AjazzManager::getInstance()->devices)
    {
        targetDevice->addOption(d->modelName + " - " + d->serialNumber, d->serialNumber);
    }
    
    if (targetDevice->getAllKeys().contains(currentSerial))
    {
        targetDevice->setValueWithKey(currentSerial);
    }
    else if (AjazzManager::getInstance()->devices.size() > 0 && currentSerial == "none")
    {
        targetDevice->setValueWithKey(AjazzManager::getInstance()->devices[0]->serialNumber);
    }
    else
    {
        targetDevice->setValueWithKey("none");
    }
}

void AjazzModule::syncGeometry(DeviceType type, bool rebuildUI)
{
    switch (type)
    {
    case AKP153:
        numRows = 3;
        numColumns = 5;
        numSideKeys = 3;
        deviceWarning->hideInEditor = true;
        break;
    case AKP03:
        numRows = 2;
        numColumns = 3;
        numSideKeys = 3;
        deviceWarning->hideInEditor = false;
        break;
    case AKP05:
        numRows = 2;
        numColumns = 5;
        numSideKeys = 4;
        deviceWarning->hideInEditor = false;
        break;
    }

    if (rebuildUI)
    {
        queuedNotifier.addMessage(new ContainerAsyncEvent(ContainerAsyncEvent::ControllableContainerNeedsRebuild, &moduleParams));
        rebuildValues();
    }
}

void AjazzModule::syncGeometryFromDevice(AjazzDevice* newDevice, bool rebuildUI)
{
    if (newDevice == nullptr) return;

    DeviceType type = AKP153;
    if (newDevice->numRows == 2 && newDevice->numColumns == 3) type = AKP03;
    else if (newDevice->numRows == 2 && newDevice->numColumns == 5) type = AKP05;

    if (deviceType->getValueDataAsEnum<DeviceType>() != type)
        deviceType->setValueWithData(type);
    else
        syncGeometry(type, rebuildUI);
}

void AjazzModule::rebuildValues()
{
	while (buttonStates.size() > numRows)
	{
		buttonStates.remove(numRows);
		valuesCC.removeChildControllableContainer(buttonRowsCC[numRows]);
		buttonRowsCC.remove(numRows);

		colors.remove(numRows);
		colorsCC.removeChildControllableContainer(colorsCC.controllableContainers[numRows]);

		images.remove(numRows);
		imagesCC.removeChildControllableContainer(imagesCC.controllableContainers[numRows]);

		texts.remove(numRows);
		textsCC.removeChildControllableContainer(textsCC.controllableContainers[numRows]);
	}

	while (buttonStates.size() < numRows)
	{
		buttonStates.add(new Array<BoolParameter*>());
		ControllableContainer* btCC = new ControllableContainer("Row " + String(buttonStates.size()));
		buttonRowsCC.add(btCC);
		valuesCC.addChildControllableContainer(btCC, true);

		colors.add(new Array<ColorParameter*>());
		ControllableContainer* cCC = new ControllableContainer("Row " + String(colors.size()));
		colorsCC.addChildControllableContainer(cCC, true);

		images.add(new Array<FileParameter*>());
		ControllableContainer* iCC = new ControllableContainer("Row " + String(images.size()));
		imagesCC.addChildControllableContainer(iCC, true);

		texts.add(new Array<StringParameter*>());
		ControllableContainer* tCC = new ControllableContainer("Row " + String(texts.size()));
		textsCC.addChildControllableContainer(tCC, true);
	}

	for (int r = 0; r < numRows; r++)
	{
		Array<BoolParameter*>* btStates = buttonStates[r];
		Array<ColorParameter*>* cColors = colors[r];
		Array<FileParameter*>* cImages = images[r];
		Array<StringParameter*>* cTexts = texts[r];
		ControllableContainer* btCC = buttonRowsCC[r];
		ControllableContainer* cCC = colorsCC.controllableContainers[r];
		ControllableContainer* iCC = imagesCC.controllableContainers[r];
		ControllableContainer* tCC = textsCC.controllableContainers[r];

		while (btStates->size() > numColumns)
		{
			btStates->remove(numColumns);
			btCC->removeControllable(btCC->controllables[numColumns]);

			cColors->remove(numColumns);
			cCC->removeControllable(cCC->controllables[numColumns]);

			cImages->remove(numColumns);
			iCC->removeControllable(iCC->controllables[numColumns]);

			cTexts->remove(numColumns);
			tCC->removeControllable(tCC->controllables[numColumns]);
		}

		while (btStates->size() < numColumns)
		{
			String btId = String(btStates->size() + 1);
			btStates->add(btCC->addBoolParameter("Button " + btId, "Is row " + String(r + 1) + ", button " + btId + " pressed ?", false));
			cColors->add(cCC->addColorParameter("Color " + btId, "If not image, color for row " + String(r + 1) + ", button " + btId, Colours::black));
			cImages->add(iCC->addFileParameter("Image " + btId, "Image for row " + String(r + 1) + ", button" + btId));
			cTexts->add(tCC->addStringParameter("Text " + btId, "Text for row " + String(r + 1) + ", button" + btId, ""));
		}
	}
    
    while (sideColors.size() > numSideKeys)
    {
        int idx = sideColors.size() - 1;
        sideCC.removeControllable(sideColors[idx]);
        sideCC.removeControllable(sideImages[idx]);
        sideCC.removeControllable(sideTexts[idx]);
        sideColors.remove(idx);
        sideImages.remove(idx);
        sideTexts.remove(idx);
    }

    while (sideColors.size() < numSideKeys)
    {
        int i = sideColors.size() + 1;
        sideColors.add(sideCC.addColorParameter("Color " + String(i), "Color for side button " + String(i), Colours::black));
        sideImages.add(sideCC.addFileParameter("Image " + String(i), "Image for side button " + String(i)));
        sideTexts.add(sideCC.addStringParameter("Text " + String(i), "Text for side button " + String(i), ""));
    }
}

void AjazzModule::setDevice(AjazzDevice* newDevice)
{
	if (newDevice == device) return;
	if (device != nullptr)
	{
		device->removeAjazzListener(this);
	}

	device = newDevice;
	readyToSend = false;

	if (device != nullptr)
	{
        syncGeometryFromDevice(device, true);
		device->addAjazzListener(this);

		// Defer initialization: give the device a moment to settle after HID open
		// before sending any output reports.
		WeakReference<AjazzModule> weakThis(this);
		Timer::callAfterDelay(300, [weakThis]()
		{
			if (weakThis == nullptr || weakThis->device == nullptr) return;
			weakThis->device->setBrightness(weakThis->brightness->intValue());
			weakThis->device->reset();
			// Give the device time to process the reset before sending images
			Timer::callAfterDelay(200, [weakThis]()
			{
				if (weakThis == nullptr || weakThis->device == nullptr) return;
				weakThis->readyToSend = true;
				for (int i = 0; i < weakThis->numRows; ++i)
					for (int j = 0; j < weakThis->numColumns; j++)
						weakThis->updateButton(i, j);
				for (int i = 0; i < weakThis->numSideKeys; i++)
					weakThis->updateSideButton(i);
			});
		});
	}

	isConnected->setValue(device != nullptr);
}

void AjazzModule::updateButton(int row, int column)
{
	if (device == nullptr || !readyToSend) return;
	if (row < 0 || row >= numRows || column < 0 || column >= numColumns) return;

	File f = (*images[row])[column]->getFile();
    String overlayText = (*texts[row])[column]->stringValue();
    Colour c = (*colors[row])[column]->getColor();
    bool highlighted = highlightPressedButtons->boolValue() ? (*buttonStates[row])[column]->boolValue() : false;

	Image image;
	if (f.existsAsFile())
	{
		image = ImageFileFormat::loadFrom(f);
		if (image.isNull())
			NLOG("Ajazz Module", "Image load failed for: " << f.getFullPathName());
	}

	if (!image.isNull())
	{
		if (logOutgoingData->boolValue()) NLOG("Ajazz Module", "Sending image for (" << row << "," << column << "): " << f.getFileName() << " " << image.getWidth() << "x" << image.getHeight());
		device->setImage(row, column, image, highlighted, overlayText, textSize->intValue(), colorizeImages->boolValue(), c);
	}
	else
	{
		Image img(Image::RGB, 85, 85, true);
        Graphics g(img);
        g.fillAll(c);
		device->setImage(row, column, img, highlighted, overlayText, textSize->intValue());
	}

	outActivityTrigger->trigger();
}

void AjazzModule::updateSideButton(int index)
{
    if (device == nullptr || !readyToSend) return;
    if (index < 0 || index >= numSideKeys) return;
    
    File f = sideImages[index]->getFile();
    String overlayText = sideTexts[index]->stringValue();
    Colour c = sideColors[index]->getColor();
    bool highlighted = false;
    
    Image image;
    if (f.existsAsFile())
    {
        image = ImageFileFormat::loadFrom(f);
        if (image.isNull())
            NLOG("Ajazz Module", "Side image load failed for: " << f.getFullPathName());
    }

    if (!image.isNull())
    {
        if (logOutgoingData->boolValue()) NLOG("Ajazz Module", "Sending side image for " << index << ": " << f.getFileName() << " " << image.getWidth() << "x" << image.getHeight());
        device->setSideImage(index, image, highlighted, overlayText, textSize->intValue(), colorizeImages->boolValue(), c);
    }
    else
    {
        Image img(Image::RGB, 85, 85, true);
        Graphics g(img);
        g.fillAll(c);
        device->setSideImage(index, img, highlighted, overlayText, textSize->intValue());
    }
    
    outActivityTrigger->trigger();
}

void AjazzModule::setColor(int row, int column, const Colour& c)
{
	if (row < 0 || row >= numRows) return;
	if (column < 0 || column >= numColumns) return;
	(*colors[row])[column]->setColor(c);
}

void AjazzModule::setAllColor(const Colour& color)
{
	for (auto& cc : colors)
	{
		for (auto& c : *cc) c->setColor(color);
	}
}

void AjazzModule::setImage(int row, int column, const String& path)
{
	if (row < 0 || row >= numRows) return;
	if (column < 0 || column >= numColumns) return;
	(*images[row])[column]->setValue(path);
}

void AjazzModule::setText(int row, int column, const String& text)
{
	if (row < 0 || row >= numRows) return;
	if (column < 0 || column >= numColumns) return;
	(*texts[row])[column]->setValue(text);
}

void AjazzModule::clearTexts()
{
	for (auto& tt : texts)
	{
		for (auto& t : *tt) t->setValue("");
	}
}

void AjazzModule::setSideColor(int index, const Colour& c)
{
    if (index < 0 || index >= numSideKeys) return;
    sideColors[index]->setColor(c);
}

void AjazzModule::setSideImage(int index, const String& path)
{
    if (index < 0 || index >= numSideKeys) return;
    sideImages[index]->setValue(path);
}

void AjazzModule::setSideText(int index, const String& text)
{
    if (index < 0 || index >= numSideKeys) return;
    sideTexts[index]->setValue(text);
}

void AjazzModule::ajazzButtonPressed(int row, int column)
{
	if (logIncomingData->boolValue()) NLOG(niceName, "Button " + String(column + 1) << ":" << String(row + 1) + " pressed");
	if (row >= numRows || column >= numColumns) return;

	inActivityTrigger->trigger();
	(*buttonStates[row])[column]->setValue(true);
	if (highlightPressedButtons->boolValue()) updateButton(row, column);
}

void AjazzModule::ajazzButtonReleased(int row, int column)
{
	if (logIncomingData->boolValue()) NLOG(niceName, "Button " + String(column + 1) << ":" << String(row + 1) + " released");
	if (row >= numRows || column >= numColumns) return;

	inActivityTrigger->trigger();
	(*buttonStates[row])[column]->setValue(false);
	if (highlightPressedButtons->boolValue()) updateButton(row, column);
}


void AjazzModule::onControllableFeedbackUpdateInternal(ControllableContainer* cc, Controllable* c)
{
	Module::onControllableFeedbackUpdateInternal(cc, c);

    if (c == targetDevice)
    {
        String serial = targetDevice->getValueData().toString();
        if (serial == "none")
        {
            setDevice(nullptr);
        }
        else
        {
            setDevice(AjazzManager::getInstance()->getItemWithSerial(serial));
        }
    }
    else if (c == deviceType)
    {
        if (device != nullptr) syncGeometryFromDevice(device, true);
        else syncGeometry(deviceType->getValueDataAsEnum<DeviceType>(), true);
    }

	if (!enabled->boolValue()) return;
	if (device == nullptr) return;

	if (c == reset || c == colorizeImages)
	{
		if (c == reset)
		{
			readyToSend = false;
			device->reset();
			// Give the device time to process the clear before sending new images
			WeakReference<AjazzModule> weakThis(this);
			Timer::callAfterDelay(200, [weakThis]() {
				if (weakThis == nullptr || weakThis->device == nullptr) return;
				weakThis->readyToSend = true;
				for (int i = 0; i < weakThis->numRows; ++i)
					for (int j = 0; j < weakThis->numColumns; j++)
						weakThis->updateButton(i, j);
				for (int i = 0; i < weakThis->numSideKeys; i++)
					weakThis->updateSideButton(i);
			});
		}
		else
		{
			for (int i = 0; i < numRows; ++i)
				for (int j = 0; j < numColumns; j++)
					updateButton(i, j);
			for (int i = 0; i < numSideKeys; i++)
				updateSideButton(i);
		}
	}
	else if (c == brightness)
	{
		device->setBrightness(brightness->intValue());
	}
	else if (c == textSize)
	{
		for (int i = 0; i < numRows; ++i)
			for (int j = 0; j < numColumns; j++)
				updateButton(i, j);
		for (int i = 0; i < numSideKeys; i++)
			updateSideButton(i);
	}
	else if (c->parentContainer->parentContainer == &colorsCC)
	{
		int row = colorsCC.controllableContainers.indexOf(c->parentContainer);
		int column = colors[row]->indexOf((ColorParameter*)c);
		updateButton(row, column);
	}
	else if (c->parentContainer->parentContainer == &imagesCC)
	{
		int row = imagesCC.controllableContainers.indexOf(c->parentContainer);
		int column = images[row]->indexOf((FileParameter*)c);
		updateButton(row, column);
	}
	else if (c->parentContainer->parentContainer == &textsCC)
	{
		int row = textsCC.controllableContainers.indexOf(c->parentContainer);
		int column = texts[row]->indexOf((StringParameter*)c);
		updateButton(row, column);
	}
    else if (c->parentContainer == &sideCC)
    {
        if (sideColors.contains((ColorParameter*)c)) updateSideButton(sideColors.indexOf((ColorParameter*)c));
        else if (sideImages.contains((FileParameter*)c)) updateSideButton(sideImages.indexOf((FileParameter*)c));
        else if (sideTexts.contains((StringParameter*)c)) updateSideButton(sideTexts.indexOf((StringParameter*)c));
    }
}

void AjazzModule::deviceAdded(AjazzDevice* d)
{
    updateDeviceList();
}

void AjazzModule::deviceRemoved(AjazzDevice* d)
{
    if (device == d) setDevice(nullptr);
    updateDeviceList();
}

void AjazzModule::afterLoadJSONDataInternal()
{
	Module::afterLoadJSONDataInternal();
}
