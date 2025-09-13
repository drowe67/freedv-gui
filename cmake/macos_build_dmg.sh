#!/bin/sh

SOURCE_FOLDER=$1

# Note: retry logic from https://github.com/darktable-org/darktable/pull/16394/files

# When building on github runner, 'hdiutil create' occasionally fails (resource busy)
# so we make several retries
try_count=0
hdiutil_success=0

while [ $hdiutil_success -ne 1 -a $try_count -lt 8 ]; do
    # Create temporary rw image
    if hdiutil create -srcfolder $SOURCE_FOLDER/ -volname FreeDV -format UDZO -fs HFS+ ./FreeDV.dmg
    then
        hdiutil_success=1
        break
    fi
    try_count=$(( $try_count + 1 ))
    echo "'hdiutil create' failed (attempt ${try_count}). Retrying..."
    sleep 1
done

if [ $hdiutil_success -ne 1 -a -n "${GITHUB_RUN_ID}" ]; then
    # Still no success after 8 attempts.
    # If we are on github runner, kill the Xprotect service and make one
    # final attempt.
    # see https://github.com/actions/runner-images/issues/7522
    echo "Killing XProtect..."
    sudo pkill -9 XProtect >/dev/null || true;
    sleep 3

    if hdiutil create -srcfolder $SOURCE_FOLDER/ -volname FreeDV -format UDZO -fs HFS+ ./FreeDV.dmg
    then
        hdiutil_success=1
    fi
fi

if [ $hdiutil_success -ne 1 ]; then
    echo "FATAL: 'hdiutil create' FAILED!"
    exit 1
fi 
