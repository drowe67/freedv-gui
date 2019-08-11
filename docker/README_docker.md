# Building freedv-gui for Windows using docker

## Requirements
* docker
* docker-compose

## Building the docker images
Building is only required once
```bash
docker-compose -f docker-compose-win.yml build
```
## Running the image(s)
```bash
docker-compose -f docker-compose-win.yml up 
```

You can optionally control via the environment variables GIT_REPO and GIT_REF which branch/commit from which repo is being built. The if these are not defined default is the `master` branch  of (https://github.com/drowe67/freedv-gui.git) and does not have to be specified explicitly.

```bash
export GIT_REF=my-super-branch
export GIT_REPO=http://github.com/dummy/freedv-gui.git
docker-compose -f docker-compose-win.yml up 

```
## Accessing created output
If you started the docker services using `docker up`, you can easily access the compiled output from the docker container.  Using the .exe file name on the last line of the Docker log:

```
docker cp fdv_win_fed28_c:/home/build/freedv-gui/build_windows/FreeDV-1.4.0-devel-20190725-7cd03db-win64.exe .
```
