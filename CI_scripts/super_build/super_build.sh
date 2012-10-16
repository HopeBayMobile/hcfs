#!/bin/bash -x
echo "************************"
echo "By calling this script, it will (a) git pull, (b) run StorageAppliance/CI_scripts/build_scripts/build_gw_package.sh"
echo "Usage: ./super_build.sh <git_branch> <build_num>"
echo "************************"

# define a function
check_ok() {
    if [ $? -ne 0 ];
    then
        echo "Execution encountered an error."
        exit 0
    fi
}
#----------------------------------------------


# Make sure only root can run this script
if [ "$(id -u)" -ne "0" ]; then echo "This script must be run as root, use 'sudo'" 1>&2
   exit 1
fi

# make sure input arguments are correct
    if [ $# -ne 2 ]
    then
        echo "Need to input 2 arguments."
        exit 1
    fi

# define parameters
    BRANCH=$1
    BUILDNUM=$2
    GIT_SRC="https://github.com/Delta-Cloud/StorageAppliance.git"
    INITPATH=$(pwd)
    
# pull code from github
    if [ ! -d StorageAppliance ]; then    # if StorageAppliance is not here
        MODE="full"
    else
        MODE="fast"
    fi

    if [ $MODE = "full" ];    then
        expect -c "
        spawn git clone $GIT_SRC
        expect \"Username for\"
        sleep 1
        send \"dc-cds\r\"
        expect \"Password for\"
        sleep 1
        send \"delta168cloud\r\"
        interact
        "
        check_ok
        cd StorageAppliance
        git stash
        git checkout $BRANCH
    else
        cd StorageAppliance
        git stash
        git checkout $BRANCH
        git reset --hard HEAD
        expect -c "
        spawn git pull
        expect \"Username for\"
        sleep 1
        send \"dc-cds\r\"
        expect \"Password for\"
        sleep 1
        send \"delta168cloud\r\"
        interact
        "
        check_ok
    fi

# re-arrange folders for build
    cp -r $INITPATH/StorageAppliance/CI_scripts/build_scripts ./
    mv $INITPATH/StorageAppliance build_scripts/
    
