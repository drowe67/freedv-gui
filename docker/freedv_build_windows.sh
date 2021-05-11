#!/bin/bash

function print_help {
    echo
    echo "Build freedv-gui for Windows using Docker"
    echo
    echo "  usage ./freedv_build_windows.sh [-d] [--noclean] [--build] [--repo GitRepo] [--branch GitBranch] 32|64"
    echo
    echo "    -d                  debug mode; trace script execution"
    echo "    --noclean           start from a previous build (git pull && make), which is faster for small changes."
    echo "                        The default is a clean build from a fresh git clone (slow but safer)"
    echo "    --build             Rebuild docker image first (run if you have modifed the docker scripts)"
    echo "    --repo GitRepo      (default https://github.com/drowe67/freedv-gui.git)"
    echo "    --branch GitBranch  (default master)"
    echo "    --bootstrap-wx      Builds wxWidgets from source (may take significantly longer to complete)"
    echo
    exit
}

# defaults - these variables are passed to the docker container
FDV_CLEAN=1
FDV_BUILD=""
FDV_GIT_REPO=https://github.com/drowe67/freedv-gui.git
FDV_GIT_BRANCH=master
FDV_BOOTSTRAP_WX=0

POSITIONAL=()
while [[ $# -gt 0 ]]; do
key="$1"
case $key in
    -d)
        set -x	
        shift
    ;;
    --noclean)
        FDV_CLEAN=0	
        shift
    ;;
    --build)
        FDV_BUILD="--build --force-recreate"	
        shift
    ;;
    --repo)
        FDV_GIT_REPO="$2"	
        shift
        shift
    ;;
    --branch)
        FDV_GIT_BRANCH="$2"	
        shift
        shift
    ;;
    --bootstrap-wx)
        FDV_BOOTSTRAP_WX=1
        shift
    ;;
    -h|--help)
        print_help	
    ;;
    *)
    POSITIONAL+=("$1") # save it in an array for later
    shift
    ;;
esac
done
set -- "${POSITIONAL[@]}" # restore positional parameters

FDV_CMAKE=mingw64-cmake
if [ $# -eq 1 ]; then
    if [ $1 -eq 32 ]; then
        FDV_CMAKE=mingw32-cmake
    fi
else
    print_help
fi

log=build_log.txt

FDV_CLEAN=$FDV_CLEAN FDV_BOOTSTRAP_WX=$FDV_BOOTSTRAP_WX FDV_CMAKE=$FDV_CMAKE FDV_GIT_REPO=$FDV_GIT_REPO FDV_GIT_BRANCH=$FDV_GIT_BRANCH docker-compose -f docker-compose-win.yml up --remove-orphans $FDV_BUILD > $log
package_docker_path=$(cat $log | sed  -n "s/.*package: \(.*exe\) .*/\1/p")
echo $package_docker_path
docker cp fdv_win_fed34_c:$package_docker_path .
