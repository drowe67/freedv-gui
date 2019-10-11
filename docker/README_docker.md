# Building freedv-gui for Windows using Docker on Ubuntu 18

## Docker installation
```
sudo apt install docker docker-compose
sudo systemctl start docker
sudo systemctl enable docker
sudo systemctl status docker
sudo usermod -aG docker $USER
<log out and log in to update groups>
groups
<you should see docker as one of your groups>
docker info
docker container run hello-world
```

## Building the docker images
Building is only required once
```
docker-compose -f docker-compose-win.yml build
```
## Running the image(s)
```
docker-compose -f docker-compose-win.yml up 
```

You can optionally control via the environment variables GIT_REPO and GIT_REF which branch/commit from which repo is being built. The if these are not defined default is the `master` branch  of (https://github.com/drowe67/freedv-gui.git) and does not have to be specified explicitly.

```bash
export GIT_REF=my-super-branch
export GIT_REPO=http://github.com/dummy/freedv-gui.git
docker-compose -f docker-compose-win.yml up 

```
## Accessing created output
If you started the docker services using `docker up`, you can easily access the compiled output from the docker container.  Using the .exe file name on the last line of the Docker log, for example:

```
docker cp fdv_win_fed30_c:/home/build/freedv-gui/build_win64/FreeDV-1.4.0-devel-20190725-7cd03db-win64.exe .
```
