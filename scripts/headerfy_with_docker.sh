#Run this script from the base directory

docker run --rm \
    --user $(id -u):$(id -g) \
    --volume "$PWD":/usr/src/dlb \
    --workdir /usr/src/dlb \
    bscpm/headache bash ./scripts/headerfy.sh
