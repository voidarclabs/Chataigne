/*
  ==============================================================================

    AjazzManager.h
    Created: 7 Apr 2026
    Author:  Andrea Bergamasco [@vjandrea]

  ==============================================================================
*/

#pragma once

#include "AjazzDevice.h"

class AjazzManager :
	public Thread
{
public:
	juce_DeclareSingleton(AjazzManager, true)

	AjazzManager();
	~AjazzManager();

	OwnedArray<AjazzDevice> devices;

	AjazzDevice* getItemWithSerial(StringRef serial);
	AjazzDevice* getItemWithPath(StringRef path);

	virtual void run() override;

	class AjazzManagerListener
	{
	public:
		virtual ~AjazzManagerListener() {}
		virtual void deviceAdded(AjazzDevice *) = 0;
		virtual void deviceRemoved(AjazzDevice *) = 0;
	};

	ListenerList<AjazzManagerListener> deviceManagerListeners;
	void addAjazzManagerListener(AjazzManagerListener* newListener) { deviceManagerListeners.add(newListener); }
	void removeAjazzManagerListener(AjazzManagerListener* listener) { deviceManagerListeners.remove(listener); }

	class AjazzManagerEvent
	{
	public:
		enum Type { DEVICES_CHANGED };
		AjazzManagerEvent(Type type) : type(type) {}
		Type type;
	};

	QueuedNotifier<AjazzManagerEvent> queuedNotifier;
	typedef QueuedNotifier<AjazzManagerEvent>::Listener AsyncListener;
	void addAsyncManagerListener(AsyncListener* newListener) { queuedNotifier.addListener(newListener); }
	void addAsyncCoalescedManagerListener(AsyncListener* newListener) { queuedNotifier.addAsyncCoalescedListener(newListener); }
	void removeAsyncManagerListener(AsyncListener* listener) { queuedNotifier.removeListener(listener); }

private:
	struct EnumeratedDevice { String path; String serial; int vid; int pid; };

	void applyEnumerationResults(Array<EnumeratedDevice> found);
	AjazzDevice* openDevice(const EnumeratedDevice& e);

	JUCE_DECLARE_WEAK_REFERENCEABLE(AjazzManager)
};
