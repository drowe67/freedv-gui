#!/bin/bash

FDV_CURRENT_BRANCH=`git rev-parse --abbrev-ref HEAD 2>/dev/null`
FDV_CURRENT_BRANCH=${FDV_CURRENT_BRANCH:=master}
FDV_CURRENT_REPO=`git remote get-url origin 2>/dev/null`
FDV_CURRENT_REPO=${FDV_CURRENT_REPO:=https://github.com/drowe67/freedv-gui.git}

function print_help {
    echo
    echo "Build freedv-gui for Windows using Docker"
    echo
    echo "  usage ./freedv_build_windows.sh [-d] [--noclean] [--build] [--repo GitRepo] [--branch GitBranch] 32|64"
    echo
    echo "    -d                  debug mode; trace script execution"
    echo "    --noclean           start from a previous build (git pull && make), which is faster for small changes."
    echo "                        The default is a clean build from a fresh git clone (slow but safer)"
    echo "    --build             Update docker image first (run if you have modified the docker scripts in fdv_win_fedora)"
    echo "    --rebuild           Completely recreate docker image first (e.g. run if you have new rpm packages)"
    echo "    --repo GitRepo      (default $FDV_CURRENT_REPO)"
    echo "    --branch GitBranch  (default $FDV_CURRENT_BRANCH)"
    echo "    --bootstrap-wx      Builds wxWidgets from source (may take significantly longer to complete)"
    echo
    exit
}

# defaults - these variables are passed to the docker container
FDV_CLEAN=1
FDV_BUILD=0
FDV_REBUILD=0
FDV_GIT_REPO=$FDV_CURRENT_REPO
FDV_GIT_BRANCH=$FDV_CURRENT_BRANCH
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
        FDV_BUILD=1
        shift
    ;;
    --rebuild)
        FDV_REBUILD=1
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

# create log file
log=build_log.txt
echo > $log

if [ $FDV_BUILD -eq 1 ]; then
    docker-compose -f docker-compose-win.yml build >> $log
fi

if [ $FDV_REBUILD -eq 1 ]; then
    docker-compose -f docker-compose-win.yml rm -f >> $log
    docker-compose -f docker-compose-win.yml build --no-cache >> $log
fi

FDV_CLEAN=$FDV_CLEAN FDV_BOOTSTRAP_WX=$FDV_BOOTSTRAP_WX FDV_CMAKE=$FDV_CMAKE FDV_GIT_REPO=$FDV_GIT_REPO FDV_GIT_BRANCH=$FDV_GIT_BRANCH \
docker-compose -f docker-compose-win.yml up --remove-orphans >> $log
package_docker_path=$(cat $log | sed  -n "s/.*package: \(.*exe\) .*/\1/p")
echo $package_docker_path
docker cp fdv_win_fed34_c:$package_docker_path .
