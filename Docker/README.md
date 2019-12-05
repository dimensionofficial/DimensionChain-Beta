# Run in docker

Simple and fast setup of dimension on Docker is also available.

## Install Dependencies


- [Docker](https://docs.docker.com) Docker 17.05 or higher is required
- [docker-compose](https://docs.docker.com/compose/) version >= 1.10.0

## Docker Requirement

- At least 7GB RAM (Docker -> Preferences -> Advanced -> Memory -> 7GB or above)
- If the build below fails, make sure you've adjusted Docker Memory settings and try again.  

## Build dimension image

```bash
git clone https://github.com/dimensionofficial/dimension.git --recursive  --depth 1
cd dimension/Docker
docker build . -t dimension/dimension
```


The above will build off the most recent commit to the master branch by default. If you would like to target a specific branch/tag, you may use a build argument. For example, if you wished to generate a docker image based off of the v1.7.0 tag, you could do the following:

```bash
docker build -t eonio/dimension:v1.7.0 --build-arg branch=v1.7.0 .
```

By default, the symbol in eonio.system is set to EON. You can override this using the symbol argument while building the docker image.

```bash
docker build -t dimension/dimension --build-arg symbol=<symbol> .
```

## Start nodeon docker container only


```bash
docker run --name nodeon -p 8888:8888 -p 9876:9876 -t eonio/eon nodeond.sh -e --http-alias=nodeon:8888 --http-alias=127.0.0.1:8888 --http-alias=localhost:8888 arg1 arg2
```


By default, all data is persisted in a docker volume. It can be deleted if the data is outdated or corrupted:

```bash
$ docker inspect --format '{{ range .Mounts }}{{ .Name }} {{ end }}' nodeon
fdc265730a4f697346fa8b078c176e315b959e79365fc9cbd11f090ea0cb5cbc
$ docker volume rm fdc265730a4f697346fa8b078c176e315b959e79365fc9cbd11f090ea0cb5cbc
```

Alternately, you can directly mount host directory into the container

```bash
docker run --name nodeon -v /path-to-data-dir:/opt/eonio/bin/data-dir -p 8888:8888 -p 9876:9876 -t eonio/eon nodeond.sh -e --http-alias=nodeon:8888 --http-alias=127.0.0.1:8888 --http-alias=localhost:8888 arg1 arg2
```

## Get chain info

```bash
curl http://127.0.0.1:8888/v1/chain/get_info
```

## Start both nodeon and keond containers

```bash
docker volume create --name=nodeon-data-volume
docker volume create --name=keond-data-volume
docker-compose up -d
```

After `docker-compose up -d`, two services named `nodeond` and `keond` will be started. nodeon service would expose ports 8888 and 9876 to the host. keond service does not expose any port to the host, it is only accessible to cleon when running cleon is running inside the keond container as described in "Execute cleon commands" section.

### Execute cleon commands

You can run the `cleon` commands via a bash alias.

```bash
alias cleon='docker-compose exec keond /opt/dimension/bin/cleon -u http://nodeond:8888 --wallet-url http://localhost:8900'
cleon get info
cleon get account inita
```

Upload sample exchange contract

```bash
cleon set contract exchange contracts/exchange/
```

If you don't need keond afterwards, you can stop the keond service using

```bash
docker-compose stop keond
```

### Develop/Build custom contracts

Due to the fact that the eonio/dimension image does not contain the required dependencies
