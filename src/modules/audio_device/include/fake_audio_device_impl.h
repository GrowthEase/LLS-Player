#ifndef FAKE_AUDIO_DEVICE_IMPL_H_
#define FAKE_AUDIO_DEVICE_IMPL_H_

#include <stdint.h>

#include "modules/audio_device/include/audio_device.h"

namespace webrtc {

class AudioDeviceGeneric;
class AudioManager;
class AudioLayer;

class FakeAudioDeviceImpl : public AudioDeviceModule {

public:
	FakeAudioDeviceImpl() {}
	virtual ~FakeAudioDeviceImpl() {}

    // Retrieve the currently utilized audio layer
    virtual int32_t ActiveAudioLayer(AudioLayer* audioLayer) const override { return -1; }

    // Full-duplex transportation of PCM audio
    virtual int32_t RegisterAudioCallback(AudioTransport* audioCallback) override { return -1; }

    // Main initialization and termination
    virtual int32_t Init() override{ return 0; }
    virtual int32_t Terminate() override{ return -1; }
    virtual bool Initialized() const override { return false; }

    // Device enumeration
    virtual int16_t PlayoutDevices() override { return -1; }
    virtual int16_t RecordingDevices() override { return -1; }
    virtual int32_t PlayoutDeviceName(uint16_t index,
        char name[kAdmMaxDeviceNameSize],
        char guid[kAdmMaxGuidSize]) override {
        return -1;
    }
    virtual int32_t RecordingDeviceName(uint16_t index,
        char name[kAdmMaxDeviceNameSize],
        char guid[kAdmMaxGuidSize]) override {
        return -1;
    }

    // Device selection
    virtual int32_t SetPlayoutDevice(uint16_t index) override { return -1; }
    virtual int32_t SetPlayoutDevice(WindowsDeviceType device) override { return -1; }
    virtual int32_t SetRecordingDevice(uint16_t index) override { return -1; }
    virtual int32_t SetRecordingDevice(WindowsDeviceType device) override { return -1; }

    // Audio transport initialization
    virtual int32_t PlayoutIsAvailable(bool* available) override { return -1; }
    virtual int32_t InitPlayout() override { return -1; }
    virtual bool PlayoutIsInitialized() const override { return false; }
    virtual int32_t RecordingIsAvailable(bool* available) override { return -1; }
    virtual int32_t InitRecording() override { return -1; }
    virtual bool RecordingIsInitialized() const override { return false; }

    // Audio transport control
    virtual int32_t StartPlayout() override { return -1; }
    virtual int32_t StopPlayout() override { return -1; }
    virtual bool Playing() const override { return false; }
    virtual int32_t StartRecording() override { return -1; }
    virtual int32_t StopRecording() override { return -1; }
    virtual bool Recording() const override { return false; }

    // Audio mixer initialization
    virtual int32_t InitSpeaker() override { return -1; }
    virtual bool SpeakerIsInitialized() const override { return false; }
    virtual int32_t InitMicrophone() override { return -1; }
    virtual bool MicrophoneIsInitialized() const override { return false; }

    // Speaker volume controls
    virtual int32_t SpeakerVolumeIsAvailable(bool* available) override { return -1; }
    virtual int32_t SetSpeakerVolume(uint32_t volume) override { return -1; }
    virtual int32_t SpeakerVolume(uint32_t* volume) const override { return -1; }
    virtual int32_t MaxSpeakerVolume(uint32_t* maxVolume) const override { return -1; }
    virtual int32_t MinSpeakerVolume(uint32_t* minVolume) const override { return -1; }

    // Microphone volume controls
    virtual int32_t MicrophoneVolumeIsAvailable(bool* available) override { return -1; }
    virtual int32_t SetMicrophoneVolume(uint32_t volume) override { return -1; }
    virtual int32_t MicrophoneVolume(uint32_t* volume) const override { return -1; }
    virtual int32_t MaxMicrophoneVolume(uint32_t* maxVolume) const override { return -1; }
    virtual int32_t MinMicrophoneVolume(uint32_t* minVolume) const override { return -1; }

    // Speaker mute control
    virtual int32_t SpeakerMuteIsAvailable(bool* available) override { return -1; }
    virtual int32_t SetSpeakerMute(bool enable) override { return -1; }
    virtual int32_t SpeakerMute(bool* enabled) const override { return -1; }

    // Microphone mute control
    virtual int32_t MicrophoneMuteIsAvailable(bool* available) override { return -1; }
    virtual int32_t SetMicrophoneMute(bool enable) override { return -1; }
    virtual int32_t MicrophoneMute(bool* enabled) const override { return -1; }

    // Stereo support
    virtual int32_t StereoPlayoutIsAvailable(bool* available) const override { return -1; }
    virtual int32_t SetStereoPlayout(bool enable) override { return -1; }
    virtual int32_t StereoPlayout(bool* enabled) const override { return -1; }
    virtual int32_t StereoRecordingIsAvailable(bool* available) const override { return -1; }
    virtual int32_t SetStereoRecording(bool enable) override { return -1; }
    virtual int32_t StereoRecording(bool* enabled) const override { return -1; }

    // Playout delay
    virtual int32_t PlayoutDelay(uint16_t* delayMS) const override { return -1; }

    // Only supported on Android.
    virtual bool BuiltInAECIsAvailable() const override { return false; }
    virtual bool BuiltInAGCIsAvailable() const override { return false; }
    virtual bool BuiltInNSIsAvailable() const override { return false; }

    // Enables the built-in audio effects. Only supported on Android.
    virtual int32_t EnableBuiltInAEC(bool enable) override { return -1; }
    virtual int32_t EnableBuiltInAGC(bool enable) override { return -1; }
    virtual int32_t EnableBuiltInNS(bool enable) override { return -1; }
};

} // namespace webrtc


#endif // !FAKE_AUDIO_DEVICE_IMPL_H_

