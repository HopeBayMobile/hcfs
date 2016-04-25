
## Apply TeraFonn patch to Acer S58A source

TeraFonn patch is a git branch in the S58A repository on Gitlab. To apply this patch, type:

    $ git fetch origin
    $ git checkout tf/2.0.6.0xxx
    Or use git rebase if the source code is modified
    $ git checkout tf/2.0.6.0xxx
    $ git rebase master


## Build S58A image with or without TeraFonn

After TeraFonn patch has been applied, developers can use the environment
variable ENABLE_HCFS to control the image build.

* Build S58A with TeraFonn

    $ export ENABLE_HCFS=1
    $ ./build.sh -s s58a_aap_gen1 -v

    This will build an Android image with TeraFonn patch on S58A source code.
    At makefile parsing stage (before build actually occurs), all patches will
    be applied.


* Build S58A without TeraFonn

    $ export ENABLE_HCFS=0
    $ ./build.sh -s s58a_aap_gen1 -v

    This will build an Android image with the original S58A source code. At
    makefile parsing stage (before build actually occurs), reversed patches
    will be applied on all files that affect build result.
