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

# make sure there is no other thread running build
    LOCK_FILE="/tmp/runnung_build.lock"
    if [ -f $LOCK_FILE ]
    then
        echo "There is another thread doing build."
        exit 1
    fi
    touch $LOCK_FILE   # create a lock file to block concurrent build

## make sure internet is connectable
    #~ wget www.google.com
    #~ if [ $? -ne 0 ]
    #~ then
        #~ echo "Cannot connect to internet. Please check proxy's settings."
        #~ exit 1
    #~ fi
    
# define parameters
    BRANCH=$1
    BUILDNUM=$2
    GIT_SRC="ssh://weitang.hung@172.16.78.207/git/StorageAppliance.git"
    INITPATH=$(pwd)
    
# pull code from github
    if [ ! -d StorageAppliance ]; then    # if StorageAppliance is not here
        MODE="full"
    else
        MODE="fast"
    fi

    if [ $MODE = "full" ];    then
        git clone $GIT_SRC
        check_ok
        cd $INITPATH/StorageAppliance
        git stash
        git checkout $BRANCH
    else
        cd $INITPATH/StorageAppliance
        git stash
        git checkout $BRANCH
        git reset --hard HEAD
        git pull
        check_ok
    fi

    # make a tag on Git server
    cd $INITPATH/StorageAppliance/CI_scripts/build_scripts
    source build.conf
    VER="v$GW_VERSION.$BUILDNUM"
    git tag -a $VER -m "build of $VER"
    git push orgin --tags
    
# re-arrange folders for build
    rm -r $INITPATH/build_scripts
    cp -r $INITPATH/StorageAppliance/CI_scripts/build_scripts $INITPATH
    cp -rf $INITPATH/StorageAppliance $INITPATH/build_scripts/
    # run build script
    cd $INITPATH/build_scripts/
    bash build_gw_package.sh $1 $2

    rm $LOCK_FILE   # release lock file
