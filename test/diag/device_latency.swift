// Prints the latency, safety offset and buffer size CoreAudio reports for each audio device
// whose name contains the given substring (default "BlackHole").
//   swift device_latency.swift [substring]
import CoreAudio
import Foundation

let filter = CommandLine.arguments.count > 1 ? CommandLine.arguments[1] : "BlackHole"

func prop<T>(_ id: AudioObjectID, _ selector: AudioObjectPropertySelector,
             _ scope: AudioObjectPropertyScope, _ value: inout T) -> OSStatus {
    var addr = AudioObjectPropertyAddress(mSelector: selector, mScope: scope,
                                          mElement: kAudioObjectPropertyElementMain)
    var size = UInt32(MemoryLayout<T>.size)
    return AudioObjectGetPropertyData(id, &addr, 0, nil, &size, &value)
}

var addr = AudioObjectPropertyAddress(mSelector: kAudioHardwarePropertyDevices,
                                      mScope: kAudioObjectPropertyScopeGlobal,
                                      mElement: kAudioObjectPropertyElementMain)
var size: UInt32 = 0
AudioObjectGetPropertyDataSize(AudioObjectID(kAudioObjectSystemObject), &addr, 0, nil, &size)
var ids = [AudioObjectID](repeating: 0, count: Int(size) / MemoryLayout<AudioObjectID>.size)
AudioObjectGetPropertyData(AudioObjectID(kAudioObjectSystemObject), &addr, 0, nil, &size, &ids)

for id in ids {
    var nameAddr = AudioObjectPropertyAddress(mSelector: kAudioObjectPropertyName,
                                              mScope: kAudioObjectPropertyScopeGlobal,
                                              mElement: kAudioObjectPropertyElementMain)
    var name: Unmanaged<CFString>?
    var nameSize = UInt32(MemoryLayout<Unmanaged<CFString>?>.size)
    AudioObjectGetPropertyData(id, &nameAddr, 0, nil, &nameSize, &name)
    let n = (name?.takeRetainedValue() as String?) ?? ""
    if !n.contains(filter) { continue }
    var line = "\(n) (id \(id)):"
    for (label, scope) in [("in", kAudioObjectPropertyScopeInput), ("out", kAudioObjectPropertyScopeOutput)] {
        var latency: UInt32 = 0, safety: UInt32 = 0
        _ = prop(id, kAudioDevicePropertyLatency, scope, &latency)
        _ = prop(id, kAudioDevicePropertySafetyOffset, scope, &safety)
        line += " \(label) latency=\(latency) safety=\(safety);"
    }
    var bufferFrames: UInt32 = 0
    _ = prop(id, kAudioDevicePropertyBufferFrameSize, kAudioObjectPropertyScopeGlobal, &bufferFrames)
    line += " buffer=\(bufferFrames)"
    print(line)
}
