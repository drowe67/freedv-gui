#!/bin/bash -x

# Automation to cross compile freedv-gui for Windows using Docker on a
# remote machine, then extract it from Docker and upload to a web
# server
#
# usage:
# $ BUILD_MACHINE=machine_name UPLOAD_PORT=1234 UPLOAD_USER_PATH=user@mywebserver:downloads \
#   freedv_build_upload_windows.sh 32|64

[ -z $BUILD_MACHINE ] && { echo "set BUILD_MACHINE"; exit 1; }
[ -z $UPLOAD_PORT ] && { echo "set UPLOAD_PORT"; exit 1; }
[ -z $UPLOAD_USER_PATH ] && { echo "set UPLOAD_USER_PATH"; exit 1; }
[ -z $UPLOAD_USER_PATH ] && { echo "set UPLOAD_USER_PATH"; exit 1; }
[ -z $FDV_GIT_BRANCH ] && FDV_GIT_BRANCH=master

FDV_CMAKE=mingw64-cmake
if [ $# -eq 1 ]; then
    if [ $1 -eq 32 ]; then
        FDV_CMAKE=mingw32-cmake
    fi
fi

log=build_log.txt
ssh $BUILD_MACHINE "cd freedv-gui/docker; FDV_CMAKE=$FDV_CMAKE FDV_GIT_BRANCH=$FDV_GIT_BRANCH docker-compose -f docker-compose-win.yml up" > $log
package_docker_path=$(cat $log | sed  -n "s/.*package: \(.*exe\) .*/\1/p")
echo $package_docker_path
ssh $BUILD_MACHINE "docker cp fdv_win_fed28_c:$package_docker_path freedv-gui/docker"
package_file=$(basename $package_docker_path)
scp $BUILD_MACHINE:freedv-gui/docker/$package_file .
scp -P $UPLOAD_PORT $package_file $UPLOAD_USER_PATH
