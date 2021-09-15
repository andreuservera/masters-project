# cnc

### Build docker image
`docker build --rm -t <name> .`
Where name could be: aservera/cnc:latest

### Run docker compose
Before anything, make sure other running containers can't interfere by running:
`docker container prune`.

Inside the directory where docker-compose.yml is Run:
`docker-compose up -d`

Once the container is running in the background you can run docker-exec commands, such as openning a shell:

`docker exec -it cnc ./bin/bash`

Once in the shell you can execute the cnc application:

`cd /libs/cnc/build/`
`./cnc`
