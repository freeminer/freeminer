# IMAGE=ubuntu ./docker.sh
# IMAGE=debian ./docker.sh
# IMAGE=manjarolinux ./docker.sh
# IMAGE=archlinux ./docker.sh
# IMAGE=fedora ./docker.sh

docker run -v $(pwd)/..:/work -it ${IMAGE=ubuntu} env CCACHE_DIR=/work/.ccache bash -c " cd /work/build_tools/; ./build.sh "
