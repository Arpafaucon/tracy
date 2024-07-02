#!/bin/bash
#NOMERG
docker run -it --rm --mount "type=bind,source=$PWD,target=/src" registry.local.wandercraft.eu:5000/texlive:latest /bin/bash -c "cd /src/manual && make all"
