An initial Pure Data external

Just adding some sources and make them compile and work in PD.

Uses the [pd-lib-builder](https://github.com/pure-data/pd-lib-builder) Makefile.

## Usage ##

Clone and build this example via:

    git clone --recursive https://github.com/nuess0r/projectM-test.git
    cd projectM-test/pdprojectm
    make

Make sure you use the `--recursive` flag when checking out the repository so that the pd-lib-builder dependency is also checked out. If you forgot to add it, you can also manually fetch pd-lib-builder via:

    cd projectM-test/pdprojectm
    git submodule init
    git submodule update

## Build ##

You should have a copy of the pure-data and Gem source code - the following build command assumes it is in the `../pure-data` directory. [This page explains how you can get the pure-data source code](https://puredata.info/docs/developer/GettingPdSource).

By default, pd-lib-builder will attempt to auto-locate an install of Pure Data. The following command will build the external and install the distributable files into a subdirectory called `build/helloworld`.

    make install pdincludepath=../pure-data/src/ objectsdir=./build

See `make help` for more details.

## Install dependencies ##

Installation of the dependencies if you use Debian:

pdprojectm Pure Data external:
apt install build-essential gcc cmake pd Gem Gem-dev
