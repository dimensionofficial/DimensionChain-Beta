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
docker run --name nodeon -v /path-to-data-dir:/opt/eonio/bin/data-dir -p 8888:8888 -p 9876:9876 -t eonio/eon nodeosd.sh -e --http-alias=nodeon:8888 --http-alias=127.0.0.1:8888 --http-alias=localhost:8888 arg1 arg2
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

After `docker-compose up -d`, two services named `nodeond` and `keond` will be started. nodeos service would expose ports 8888 and 9876 to the host. keosd service does not expose any port to the host, it is only accessible to cleos when running cleon is running inside the keosd container as described in "Execute cleon commands" section.

### Execute cleon commands

You can run the `cleos` commands via a bash alias.

```bash
alias cleos='docker-compose exec keosd /opt/dimension/bin/cleos -u http://nodeond:8888 --wallet-url http://localhost:8900'
cleos get info
cleos get account inita
```

Upload sample exchange contract

```bash
cleos set contract exchange contracts/exchange/
```

If you don't need keosd afterwards, you can stop the keosd service using

```bash
docker-compose stop keond
```

### Develop/Build custom contracts

Due to the fact that the eosio/dimension image does not contain the required dependencies for contract development (this is by design, to keep the image size small), you will need to utilize the eosio/eos-dev image. This image contains both the required binaries and dependencies to build contracts using eoniocpp.

You can either use the image available on [Docker Hub](https://hub.docker.com/r/eonio/eon-dev/) or navigate into the dev folder and build the image manually.

```bash
cd dev
docker build -t eonio/eon-dev .
```

### Change default configuration

You can use docker compose override file to change the default configurations. For example, create an alternate config file `config2.ini` and a `docker-compose.override.yml` with the following content.

```yaml
version: "2"

services:
  nodeon:
    volumes:
      - nodeon-data-volume:/opt/eonio/bin/data-dir
      - ./config2.ini:/opt/eonio/bin/data-dir/config.ini
```

Then restart your docker containers as follows:

```bash
docker-compose down
docker-compose up
```

### Clear data-dir

The data volume created by docker-compose can be deleted as follows:

```bash
docker volume rm nodeon-data-volume
docker volume rm keosd-data-volume
```

### Docker Hub

Docker Hub images are now deprecated. New build images were discontinued on January 1st, 2019. The existing old images will be removed on June 1st, 2019.

### dimension Testnet

We can easily set up a dimension local testnet using docker images. Just run the following commands:

Note: if you want to use the mongo db plugin, you have to enable it in your `data-dir/config.ini` first.

```
# create volume
docker volume create --name=nodeon-data-volume
docker volume create --name=keosd-data-volume
# pull images and start containers
docker-compose -f docker-compose-eosio-latest.yaml up -d
# get chain info
curl http://127.0.0.1:8888/v1/chain/get_info
# get logs
docker-compose logs -f nodeosd
# stop containers
docker-compose -f docker-compose-eonio-latest.yaml down
```

The `blocks` data are stored under `--data-dir` by default, and the wallet files are stored under `--wallet-dir` by default, of course you can change these as you want.

### About MongoDB Plugin

Currently, the mongodb plugin is disabled in `config.ini` by default, you have to change it manually in `config.ini` or you can mount a `config.ini` file to `/opt/eonio/bin/data-dir/config.ini` in the docker-compose file.
