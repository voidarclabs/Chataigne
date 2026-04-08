/*
  ==============================================================================

    AjazzDevice.cpp
    Created: 7 Apr 2026
    Author:  Andrea Bergamasco [@vjandrea]

  ==============================================================================
*/

#include "Module/ModuleIncludes.h"
#include "AjazzDevice.h"

namespace
{
    constexpr int kAjazzCommandPayloadSize = 512;
    constexpr int kAjazzDataPayloadSize = 1024;
    constexpr int kAjazzReadSize = 512;
    constexpr int kAjazzHidReportId = 0;
    constexpr int kAjazzHidWriteSize = kAjazzDataPayloadSize + 1; // report id + payload
}

AjazzDevice::AjazzDevice(hid_device* device, String serialNumber, String devicePath, int pid) :
    Thread("AjazzDevice"),
    device(device),
    serialNumber(serialNumber),
    devicePath(devicePath),
    pid(pid),
    deviceInitialized(false),
    currentBrightness(100)
{
    if (pid == 0x6674 || pid == 0x1010 || pid == 0x1020 || pid == 0x3010) {
        modelName = "AKP153"; numRows = 3; numColumns = 5; numSideKeys = 3;
    } else if (pid == 0x1001 || pid == 0x1002 || pid == 0x1003 || pid == 0x3002 || pid == 0x3003) {
        modelName = "AKP03"; numRows = 2; numColumns = 3; numSideKeys = 3;
    } else if (pid == 0x3006 || pid == 0x3004 || pid == 0x3013) {
        modelName = "AKP05"; numRows = 2; numColumns = 5; numSideKeys = 4;
    } else {
        modelName = "Unknown"; numRows = 3; numColumns = 5; numSideKeys = 3;
    }
    numKeys = numRows * numColumns;

    if (device != nullptr) hid_set_nonblocking(device, 1);
    for (int i = 0; i < numKeys + numSideKeys; ++i) buttonStates.add(false);
    startThread();
}

AjazzDevice::~AjazzDevice()
{
    stopThread(500);

    if (device != nullptr)
    {
        hid_close(device);
        device = nullptr;
    }
}

void AjazzDevice::sendInitPacketIfNeeded()
{
    if (deviceInitialized) return;
    deviceInitialized = true;

    uint8_t packet[kAjazzCommandPayloadSize];

    // DIS packet: CRT\x00\x00DIS — wakes up the device display
    zeromem(packet, kAjazzCommandPayloadSize);
    packet[0] = 0x43; packet[1] = 0x52; packet[2] = 0x54; // "CRT"
    packet[5] = 0x44; packet[6] = 0x49; packet[7] = 0x53; // "DIS"
    sendPacket(packet, kAjazzCommandPayloadSize);

    // LIG 0: brightness = 0 as part of init sequence
    zeromem(packet, kAjazzCommandPayloadSize);
    packet[0] = 0x43; packet[1] = 0x52; packet[2] = 0x54; // "CRT"
    packet[5] = 0x4c; packet[6] = 0x49; packet[7] = 0x47; // "LIG"
    packet[10] = 0;
    sendPacket(packet, kAjazzCommandPayloadSize);
}

void AjazzDevice::reset()
{
    clearAll();
}

void AjazzDevice::setBrightness(int percent)
{
    sendInitPacketIfNeeded();

    uint8_t packet[kAjazzCommandPayloadSize];
    zeromem(packet, kAjazzCommandPayloadSize);

    // 0x43, 0x52, 0x54, 0x00, 0x00, 0x4c, 0x49, 0x47, 0x00, 0x00, PERCENT
    packet[0] = 0x43; packet[1] = 0x52; packet[2] = 0x54;
    packet[5] = 0x4c; packet[6] = 0x49; packet[7] = 0x47;
    currentBrightness = jlimit(0, 100, percent);
    packet[10] = (uint8_t)currentBrightness;

    sendPacket(packet, kAjazzCommandPayloadSize);
}

void AjazzDevice::clearButton(int buttonId)
{
    uint8_t packet[kAjazzCommandPayloadSize];
    zeromem(packet, kAjazzCommandPayloadSize);
    
    // 0x43, 0x52, 0x54, 0x00, 0x00, 0x43, 0x4c, 0x45, 0x00, 0x00, 0x00, BUTTON_ID
    packet[0] = 0x43; packet[1] = 0x52; packet[2] = 0x54;
    packet[5] = 0x43; packet[6] = 0x4c; packet[7] = 0x45;
    packet[11] = (uint8_t)buttonId;
    
    sendPacket(packet, kAjazzCommandPayloadSize);
}

void AjazzDevice::clearAll()
{
    clearButton(0xff);
}

static Image renderButtonImage(Image source, bool highlight, const String& overlayText, int textSize, bool colorize = false, Colour bgColor = Colours::black)
{
    const int IMG_SIZE = 96;
    Image iconImage(Image::RGB, IMG_SIZE, IMG_SIZE, true);
    Graphics g(iconImage);
    if (!source.isNull())
        g.drawImage(source, g.getClipBounds().toFloat());

    // Screen blend colorize: result = 1 - (1-image)*(1-color)
    // Black image pixels become bgColor; white pixels stay white.
    if (colorize && !source.isNull())
    {
        float cr = bgColor.getFloatRed();
        float cg = bgColor.getFloatGreen();
        float cb = bgColor.getFloatBlue();
        for (int sy = 0; sy < IMG_SIZE; ++sy)
            for (int sx = 0; sx < IMG_SIZE; ++sx)
            {
                Colour px = iconImage.getPixelAt(sx, sy);
                iconImage.setPixelAt(sx, sy, Colour::fromFloatRGBA(
                    1.0f - (1.0f - px.getFloatRed())   * (1.0f - cr),
                    1.0f - (1.0f - px.getFloatGreen()) * (1.0f - cg),
                    1.0f - (1.0f - px.getFloatBlue())  * (1.0f - cb),
                    1.0f));
            }
    }

    if (highlight) {
        g.setColour(Colours::white.withAlpha(0.2f));
        g.fillAll();
    }

    if (overlayText.isNotEmpty())
    {
        Colour textColour    = Colours::white;
        Colour outlineColour = Colours::black;

        Font font((float)textSize);
        g.setFont(font);
        // Shift down by half the descent to visually center text
        int vertOffset = (int)(font.getDescent() / 2.0f);
        auto bounds = g.getClipBounds().translated(0, vertOffset);

        // Draw contrasting outline at 3-pixel surrounding offsets for readability on any background
        g.setColour(outlineColour);
        for (int dx = -3; dx <= 3; ++dx)
            for (int dy = -3; dy <= 3; ++dy)
                if (dx != 0 || dy != 0)
                    g.drawFittedText(overlayText, bounds.translated(dx, dy), Justification::centred, 3);

        // Draw text fill on top
        g.setColour(textColour);
        g.drawFittedText(overlayText, bounds, Justification::centred, 3);
    }

    // Apply rotation + mirroring to match device transform.
    // Reference (mirajazz) uses: Rot90 (CW) + fliph + flipv = 270° CW total.
    // Combined: dest(sy, IMG_SIZE-1-sx) = src(sx, sy)
    Image transformed(Image::RGB, IMG_SIZE, IMG_SIZE, false);
    for (int sy = 0; sy < IMG_SIZE; ++sy)
        for (int sx = 0; sx < IMG_SIZE; ++sx)
            transformed.setPixelAt(sy, IMG_SIZE - 1 - sx, iconImage.getPixelAt(sx, sy));

    return transformed;
}

void AjazzDevice::setImage(int row, int column, Image image, bool highlight, const String& overlayText, int textSize, bool colorize, Colour bgColor)
{
    int buttonId = getButtonProtocolId(row, column);
    Image rendered = renderButtonImage(image, highlight, overlayText, textSize, colorize, bgColor);
    sendButtonImageData(buttonId, rendered);
}

void AjazzDevice::setSideImage(int index, Image image, bool highlight, const String& overlayText, int textSize, bool colorize, Colour bgColor)
{
    int buttonId = numKeys + 1 + index;
    Image rendered = renderButtonImage(image, highlight, overlayText, textSize, colorize, bgColor);
    sendButtonImageData(buttonId, rendered);
}

void AjazzDevice::sendButtonImageData(int buttonId, Image& img)
{
    if (Engine::mainEngine->isClearing) return;

    writeLock.enter();

    MemoryBlock jpegData;
    {
        MemoryOutputStream jpegStream(jpegData, false);
        JPEGImageFormat jpegFormat;
        jpegFormat.setQuality(0.9f);
        jpegFormat.writeImageToStream(img, jpegStream);
        jpegStream.flush();
    }

    // Data chunks must fill a full HID report (1024 bytes of data per write).
    // The device reassembles the JPEG as a contiguous stream from the report payloads.
    // Sending 512-byte sub-chunks in a 1024-byte report leaves the second half as zeros,
    // corrupting any JPEG that spans more than 512 bytes.
    uint8_t packet[kAjazzDataPayloadSize];

    // 1. Header packet
    zeromem(packet, kAjazzCommandPayloadSize);
    packet[0] = 0x43; packet[1] = 0x52; packet[2] = 0x54; // "CRT"
    packet[5] = 0x42; packet[6] = 0x41; packet[7] = 0x54; // "BAT"

    size_t dataSize = jpegData.getSize();
    packet[10] = (uint8_t)(dataSize >> 8);
    packet[11] = (uint8_t)(dataSize & 0xFF);
    packet[12] = (uint8_t)buttonId;

    sendPacket(packet, kAjazzCommandPayloadSize);

    // 2. Data packets — each chunk fills the full 1024-byte report payload
    const uint8_t* rawData = (const uint8_t*)jpegData.getData();
    size_t bytesSent = 0;
    while (bytesSent < dataSize) {
        size_t toSend = jmin((size_t)kAjazzDataPayloadSize, dataSize - bytesSent);
        zeromem(packet, kAjazzDataPayloadSize);
        memcpy(packet, rawData + bytesSent, toSend);
        sendPacket(packet, kAjazzDataPayloadSize);
        bytesSent += toSend;
    }

    // 3. Stop packet
    zeromem(packet, kAjazzCommandPayloadSize);
    packet[0] = 0x43; packet[1] = 0x52; packet[2] = 0x54; // "CRT"
    packet[5] = 0x53; packet[6] = 0x54; packet[7] = 0x50; // "STP"
    sendPacket(packet, kAjazzCommandPayloadSize);

    writeLock.exit();
}

void AjazzDevice::sendPacket(const uint8_t* data, int length)
{
    if (device == nullptr) return;

    // The device has a 1024-byte output report with Report ID 0.
    // hid_write() treats the first byte as the report ID, so we prepend 0x00
    // and pad to exactly 1025 bytes (1 report ID byte + 1024 data bytes).
    uint8_t buf[kAjazzHidWriteSize];
    zeromem(buf, kAjazzHidWriteSize);
    buf[0] = (uint8_t)kAjazzHidReportId;
    memcpy(buf + 1, data, jmin(length, kAjazzHidWriteSize - 1));

    if (hid_write(device, buf, kAjazzHidWriteSize) < 0)
        NLOGERROR("Ajazz Device", "Error writing to device");
}

int AjazzDevice::getButtonProtocolId(int row, int column)
{
    return (numColumns - 1 - column) * numRows + 1 + row;
}

void AjazzDevice::run()
{
    // Research confirms device goes dark after ~2s without a brightness packet.
    // Send LIG every 1.5s as heartbeat to keep screen alive.
    int heartbeatMs = 0;

    unsigned char data[kAjazzReadSize];
    while (!threadShouldExit() && device != nullptr)
    {
        try
        {
            int numRead = device != nullptr ? hid_read(device, data, kAjazzReadSize) : 0;
            if (numRead < 0)
            {
                NLOGERROR("Ajazz Device", "Device read failed, marking device as disconnected");
                hid_close(device);
                device = nullptr;
                return;
            }

            // Report format: "ACK\x00\x00OK\x00\x00\x00[buttonId][01=press/00=release]..."
            if (numRead >= 11 &&
                data[0] == 0x41 && data[1] == 0x43 && data[2] == 0x4b && // "ACK"
                data[5] == 0x4f && data[6] == 0x4b)                        // "OK"
            {
                int buttonId = data[9];
                bool isPress  = (data[10] == 0x01);

                if (buttonId >= 1 && buttonId <= numKeys)
                {
                    int column = numColumns - 1 - (buttonId - 1) / numRows;
                    int row    = (buttonId - 1) % numRows;
                    WeakReference<AjazzDevice> weakThis(this);
                    MessageManager::callAsync([weakThis, row, column, isPress]() {
                        if (weakThis == nullptr) return;
                        weakThis->deviceListeners.call(isPress ? &AjazzListener::ajazzButtonPressed
                                                               : &AjazzListener::ajazzButtonReleased,
                                                      row, column);
                    });
                }
                else if (buttonId > numKeys && buttonId <= numKeys + numSideKeys)
                {
                    int index = buttonId - numKeys - 1;
                    WeakReference<AjazzDevice> weakThis(this);
                    MessageManager::callAsync([weakThis, index, isPress]() {
                        if (weakThis == nullptr) return;
                        weakThis->deviceListeners.call(isPress ? &AjazzListener::ajazzSideButtonPressed
                                                               : &AjazzListener::ajazzSideButtonReleased,
                                                      index);
                    });
                }
            }
        }
        catch (std::exception e)
        {
            NLOGERROR("Ajazz Device", "Error trying to read from device");
            if (device != nullptr)
            {
                hid_close(device);
                device = nullptr;
            }
            return;
        }

        wait(10);

        // Heartbeat: keep screen alive (device sleeps after ~2s without LIG)
        heartbeatMs += 10;
        if (heartbeatMs >= 1500)
        {
            heartbeatMs = 0;
            if (deviceInitialized)
            {
                uint8_t hbPacket[kAjazzCommandPayloadSize];
                zeromem(hbPacket, kAjazzCommandPayloadSize);
                hbPacket[0] = 0x43; hbPacket[1] = 0x52; hbPacket[2] = 0x54; // "CRT"
                hbPacket[5] = 0x4c; hbPacket[6] = 0x49; hbPacket[7] = 0x47; // "LIG"
                hbPacket[10] = (uint8_t)currentBrightness;
                writeLock.enter();
                sendPacket(hbPacket, kAjazzCommandPayloadSize);
                writeLock.exit();
            }
        }
    }
}
