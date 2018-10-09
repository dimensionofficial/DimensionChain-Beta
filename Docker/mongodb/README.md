# Run Nodeos + MongoDB via Docker

This docker-compose will allow you to run nodeos and MongoDB and sync to any EOS-based network.


## Install Dependencies

- [Docker](https://docs.docker.com) Docker 17.05 or higher is required
- [docker-compose](https://docs.docker.com/compose/) version >= 1.10.0

## Docker Requirement

- At least 16GB RAM (Docker -> Preferences -> Advanced -> Memory -> 16GB or above)
- A disk capable of >= 5000 IOPS for the initial sync.

## Configure Peers

You need to configure the peers with which you would like to sync. You can do this by editing the ```nodeos/config.ini``` file.

## Start syncing

```bash
docker-compose up
```

**_Expect this process to take a very long time._**
