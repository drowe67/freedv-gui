#! /bin/bash
# Simple Utility Script for allowing debug of hardened macOS apps.
# This is useful mostly for plug-in developer that would like keep developing without turning SIP off.
# Credit for idea goes to (McMartin): https://forum.juce.com/t/apple-gatekeeper-notarised-distributables/29952/57?u=ttg
# Update 2022-03-10: Based on Fabian's feedback, add capability to inject DYLD for sanitizers.
#
# Please note:
# - Modern Logic (on M1s) uses `AUHostingService` which resides within the system thus not patchable and REQUIRES to turn-off SIP.
# - Some hosts uses separate plug-in scanning or sandboxing. 
#     if that's the case, it's required to patch those (if needed) and attach debugger to them instead.
#
# If you see `operation not permitted`, make sure the calling process has Full Disk Access.
# For example Terminal.app is showing and has Full Disk Access under System Preferences -> Privacy & Security
#
app_path=$1

if [ -z "$app_path" ];
then
    echo "You need to specify app to re-codesign!"
    exit 0
fi

# This uses local codesign. so it'll be valid ONLY on the machine you've re-signed with.
entitlements_plist=/tmp/debug_entitlements.plist
echo "Grabbing entitlements from app..."
codesign -d --entitlements - "$app_path" --xml >> $entitlements_plist || { exit 1; }
echo "Patch entitlements (if missing)..."
/usr/libexec/PlistBuddy -c "Add :com.apple.security.cs.disable-library-validation bool true" $entitlements_plist
/usr/libexec/PlistBuddy -c "Add :com.apple.security.cs.allow-unsigned-executable-memory bool true" $entitlements_plist
/usr/libexec/PlistBuddy -c "Add :com.apple.security.get-task-allow bool true" $entitlements_plist
# allow custom dyld for sanitizers...
/usr/libexec/PlistBuddy -c "Add :com.apple.security.cs.allow-dyld-environment-variables bool true" $entitlements_plist
echo "Re-applying entitlements (if missing)..."
codesign --force --options runtime --sign - --entitlements $entitlements_plist "$app_path" || { echo "codesign failed!"; }
echo "Removing temporary plist..."
rm $entitlements_plist
