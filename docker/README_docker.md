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
Building is only required once, or if you modify the docker scripts

```
cd $FREEDV_GUI/docker
docker-compose -f docker-compose-win.yml build
```

## Running the docker image to build Windows installers

```
cd $FREEDV_GUI/docker
./freedv_build_windows.sh 64
./freedv_build_windows.sh 32
```

See `build_log.txt` for a record of the build.  More options are available with:

```
./freedv_build_windows.sh -h
```
