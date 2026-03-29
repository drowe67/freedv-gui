#!/bin/bash

sudo launchctl unload /System/Library/LaunchDaemons/com.apple.cloudd.plist
sudo mdutil -a -i off
sudo launchctl unload -w /System/Library/LaunchDaemons/com.apple.metadata.mds.plist
