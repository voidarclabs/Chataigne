/*
  ==============================================================================

    AjazzModule.h
    Created: 7 Apr 2026
    Author:  Andrea Bergamasco [@vjandrea]

  ==============================================================================
*/

#pragma once

#include "AjazzManager.h"
#include "AjazzDevice.h"

class AjazzModule :
	public Module,
	public AjazzManager::AjazzManagerListener,
	public AjazzDevice::AjazzListener
{
public:
	AjazzModule(const String& name = "Ajazz AKP");
	~AjazzModule();

	BoolParameter* isConnected;
	AjazzDevice* device;

	enum DeviceType { AKP153, AKP03, AKP05 };
	EnumParameter* deviceType;
	EnumParameter* targetDevice;
	StringParameter* deviceWarning;

	int numRows;
	int numColumns;
    int numSideKeys;

	SpinLock rebuildingLock;

	Trigger* reset;
	BoolParameter* colorizeImages;
	BoolParameter* highlightPressedButtons;

	IntParameter* brightness;
	IntParameter* textSize;

	ControllableContainer colorsCC;
	OwnedArray<Array<ColorParameter*>> colors;
	ControllableContainer imagesCC;
	OwnedArray<Array<FileParameter*>> images;
	ControllableContainer textsCC;
	OwnedArray<Array<StringParameter*>> texts;

	Array<ControllableContainer*> buttonRowsCC;
	OwnedArray<Array<BoolParameter*>> buttonStates;
    
    ControllableContainer sideCC;
    Array<ColorParameter*> sideColors;
    Array<FileParameter*> sideImages;
    Array<StringParameter*> sideTexts;

	void rebuildValues();
	void updateDeviceList();
	void syncGeometry(DeviceType type, bool rebuildUI);
	void syncGeometryFromDevice(AjazzDevice* newDevice, bool rebuildUI);

	bool readyToSend;

	void setDevice(AjazzDevice* newDevice);
	void updateButton(int row, int column);
    void updateSideButton(int index);

	void setColor(int row, int column, const Colour& c);
	void setAllColor(const Colour& c);
	void setImage(int row, int column, const String& path);
	void setText(int row, int column, const String& text);
	void clearTexts();
    
    void setSideColor(int index, const Colour& c);
    void setSideImage(int index, const String& path);
    void setSideText(int index, const String& text);

	virtual void ajazzButtonPressed(int row, int column) override;
	virtual void ajazzButtonReleased(int row, int column) override;

	void onControllableFeedbackUpdateInternal(ControllableContainer* cc, Controllable* c) override;
	
	void deviceAdded(AjazzDevice* d) override;
	void deviceRemoved(AjazzDevice * d) override;

	void afterLoadJSONDataInternal() override;

	static AjazzModule * create() { return new AjazzModule(); }
	virtual String getDefaultTypeString() const override { return "Ajazz AKP"; }

	JUCE_DECLARE_WEAK_REFERENCEABLE(AjazzModule)
};
