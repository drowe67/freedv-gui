#!/bin/bash -x

# Automation to cross compile freedv-gui for Windows using Docker
#
# usage:
#   $ [FDV_GIT_REPO=your_repo] [FDV_GIT_BRANCH=your_branch] ./freedv_build_windows.sh 64|32

[ -z $FDV_GIT_BRANCH ] && FDV_GIT_BRANCH=master
[ -z $FDV_GIT_REPO ] && FDV_GIT_REPO=https://github.com/drowe67/freedv-gui.git

FDV_CMAKE=mingw64-cmake
if [ $# -eq 1 ]; then
    if [ $1 -eq 32 ]; then
        FDV_CMAKE=mingw32-cmake
    fi
fi

log=build_log.txt

# Uncomment the below if you touch any of the Docker scripts. Only need to be done once to rebuild.
#FDV_CLEAN=$FDV_CLEAN FDV_CMAKE=$FDV_CMAKE FDV_GIT_REPO=$FDV_GIT_REPO FDV_GIT_BRANCH=$FDV_GIT_BRANCH docker-compose -f docker-compose-win.yml up --build --force-recreate --remove-orphans > $log

FDV_CLEAN=$FDV_CLEAN FDV_CMAKE=$FDV_CMAKE FDV_GIT_REPO=$FDV_GIT_REPO FDV_GIT_BRANCH=$FDV_GIT_BRANCH docker-compose -f docker-compose-win.yml up --remove-orphans > $log
package_docker_path=$(cat $log | sed  -n "s/.*package: \(.*exe\) .*/\1/p")
echo $package_docker_path
docker cp fdv_win_fed34_c:$package_docker_path .
