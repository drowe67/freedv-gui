// Prints each screen's name, color space and backing scale factor.
import AppKit
for s in NSScreen.screens {
    print(s.localizedName, "|", s.colorSpace?.localizedName ?? "nil", "| backingScale", s.backingScaleFactor, "| frame", s.frame.size)
}
if NSScreen.screens.isEmpty {
    print("no screens")
}
