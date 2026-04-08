/*
  ==============================================================================

    AjazzManager.cpp
    Created: 7 Apr 2026
    Author:  Andrea Bergamasco [@vjandrea]

  ==============================================================================
*/

#include "Module/ModuleIncludes.h"
#include "AjazzManager.h"

juce_ImplementSingleton(AjazzManager)

AjazzManager::AjazzManager() :
	Thread("AjazzManager"),
	queuedNotifier(100)
{
	startThread();
}

AjazzManager::~AjazzManager()
{
	// Signal the thread to stop but don't block — hid_enumerate can't be interrupted
	// and waiting here would freeze the message thread. The callAsync lambda uses a
	// WeakReference so it's safe even if it fires after we're destroyed.
	signalThreadShouldExit();
	notify();
	clearSingletonInstance();
}

void AjazzManager::run()
{
	struct DeviceID { int vid; int pid; };
	static const DeviceID ids[] = {
		// AKP153 Series
		{ 0x154A, 0x6674 },
		{ 0x0300, 0x1010 },
		{ 0x0300, 0x1020 },
		{ 0x0300, 0x3010 },

		// AKP03 Series (6 buttons, 3 knobs/side buttons)
		{ 0x0300, 0x1001 },
		{ 0x0300, 0x1002 },
		{ 0x0300, 0x1003 },
		{ 0x0300, 0x3002 },
		{ 0x0300, 0x3003 },

		// AKP05 Series (10 buttons, 4 knobs/screens)
		{ 0x0300, 0x3006 },
		{ 0x0300, 0x3004 },
		{ 0x0300, 0x3013 }
	};

	while (!threadShouldExit())
	{
		Array<EnumeratedDevice> found;

		// Enumerate all HID devices once and filter in memory.
		// Only keep vendor-specific interfaces (usage_page >= 0xFF00) — keyboard
		// interfaces (usage_page=0x01) can't be opened without special TCC permissions
		// and aren't needed for Ajazz communication. Deduplicate by path so that
		// multiple usage entries sharing the same interface aren't opened twice.
		hid_device_info* allDevices = hid_enumerate(0, 0);
		StringArray seenPaths;
		for (hid_device_info* dInfo = allDevices; dInfo != nullptr; dInfo = dInfo->next)
		{
			if (threadShouldExit()) break;

			bool matchesVidPid = false;
			for (auto& id : ids)
			{
				if (dInfo->vendor_id == id.vid && dInfo->product_id == id.pid)
				{
					matchesVidPid = true;
					break;
				}
			}

			if (!matchesVidPid) continue;
			if (dInfo->usage_page < 0xFF00) continue; // skip keyboard/mouse interfaces

			String path(dInfo->path != nullptr ? dInfo->path : "");
			if (seenPaths.contains(path)) continue; // deduplicate by path
			seenPaths.add(path);

			String serial(dInfo->serial_number != nullptr ? dInfo->serial_number : L"");
			found.add({ path, serial, dInfo->vendor_id, dInfo->product_id });
		}
		hid_free_enumeration(allDevices);

		if (!threadShouldExit())
		{
			WeakReference<AjazzManager> weakThis(this);
			MessageManager::callAsync([weakThis, found]() mutable {
				if (weakThis != nullptr)
					weakThis->applyEnumerationResults(found);
			});
		}

		wait(2000);
	}
}

void AjazzManager::applyEnumerationResults(Array<EnumeratedDevice> found)
{
	bool changed = false;

	// Open newly discovered devices
	for (auto& e : found)
	{
		if (getItemWithPath(e.path) == nullptr
			&& (e.serial.isEmpty() || getItemWithSerial(e.serial) == nullptr))
		{
			AjazzDevice* p = openDevice(e);
			if (p != nullptr)
			{
				devices.add(p);
				deviceManagerListeners.call(&AjazzManagerListener::deviceAdded, p);
				changed = true;
			}
		}
	}

	// Remove disconnected devices
	Array<AjazzDevice*> devicesToRemove;
	for (auto& d : devices)
	{
		bool stillPresent = false;
		for (auto& e : found)
		{
			if ((d->devicePath.isNotEmpty() && e.path == d->devicePath)
				|| (d->devicePath.isEmpty() && d->serialNumber.isNotEmpty() && e.serial == d->serialNumber))
			{
				stillPresent = true;
				break;
			}
		}

		if (d->device == nullptr || !stillPresent)
			devicesToRemove.add(d);
	}

	for (auto& d : devicesToRemove)
	{
		deviceManagerListeners.call(&AjazzManagerListener::deviceRemoved, d);
		devices.removeObject(d);
		changed = true;
	}

	if (changed)
		queuedNotifier.addMessage(new AjazzManagerEvent(AjazzManagerEvent::DEVICES_CHANGED));
}

AjazzDevice* AjazzManager::getItemWithSerial(StringRef serial)
{
	for (auto& d : devices)
	{
		if (d->serialNumber == serial) return d;
	}
	return nullptr;
}

AjazzDevice* AjazzManager::getItemWithPath(StringRef path)
{
	for (auto& d : devices)
	{
		if (d->devicePath == path) return d;
	}
	return nullptr;
}

AjazzDevice* AjazzManager::openDevice(const EnumeratedDevice& e)
{
	hid_device* d = nullptr;

	if (e.path.isNotEmpty())
		d = hid_open_path(e.path.toRawUTF8());

	if (d == nullptr)
	{
		const wchar_t* serialWide = e.serial.isNotEmpty() ? e.serial.toWideCharPointer() : nullptr;
		d = hid_open(e.vid, e.pid, serialWide);
	}

	if (d == nullptr)
	{
		const wchar_t* err = hid_error(nullptr);
		NLOGERROR("Ajazz Manager", "Could not open Ajazz device"
			<< (e.path.isNotEmpty() ? " at " + e.path : "")
			<< (err != nullptr ? " : " + String(err) : ""));
		return nullptr;
	}

	NLOG("Ajazz Manager", "Ajazz device connected : " << e.serial);
	return new AjazzDevice(d, e.serial, e.path, e.pid);
}
