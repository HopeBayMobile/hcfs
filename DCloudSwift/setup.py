import os
import subprocess
import setuptools
from setuptools import setup, find_packages

WORKING_DIR = os.path.dirname(os.path.realpath(__file__))
BASEDIR = os.path.dirname(os.path.dirname(WORKING_DIR))
os.chdir(WORKING_DIR)

DELTADIR = "/etc/delta"


def isAllDebInstalled(debSrc):
    cmd = "find %s -maxdepth 1 -name \'*.deb\'  " % debSrc
    po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    lines = po.stdout.readlines()
    po.wait()

    if po.returncode != 0:
        print "Failed to execute %s" % cmd
        return -1

    returncode = 0
    devnull = open(os.devnull, "w")
    for line in lines:
        pkgname = line.split('/')[-1].split('_')[0]
        retval = subprocess.call(["dpkg", "-s", pkgname], stdout=devnull, stderr=devnull)
        if retval != 0:
            return False

    devnull.close()

    return True



def read(fname):
    return open(os.path.join(os.path.dirname(__file__), fname)).read()

def main():
    os.system("mkdir -p %s" % DELTADIR)

    setup(
        name="DCloudSwift",
        version="0.5",
        author="Cloud Data Team, CTBD, Delta Electronic Inc.",
        description=("Delta Inc. CTBD Storage Gateway"),
        license="Delta Inc.",
        keywords=['deploy', 'swift', 'cloud'],

        packages=find_packages('src'),  # include all packages under src
        package_dir={'': 'src'},   # tell distutils packages are under src
        package_data={
            # If any package contains *.txt or *.rst files, include them:
            '': ['*.txt', '*.rst', '*.sh', "*.ini"],
        },

        scripts=[
        'bin/swift-event-manager',
        'bin/swift-maintain-switcher'
        ],

        entry_points={
                    'console_scripts': [
                    'dcloud_swift_deploy = DCloudSwift.master.SwiftDeploy:deploy',
                    'dcloud_swift_addNodes = DCloudSwift.master.SwiftDeploy:addNodes',
                    'dcloud_swift_deleteNodes = DCloudSwift.master.SwiftDeploy:deleteNodes',
                    'dcloud_initialize_node_info = DCloudSwift.master.swiftEventMgr:initializeNodeInfo',
                    'dcloud_clear_node_info = DCloudSwift.master.swiftEventMgr:clearNodeInfo',
                    'dcloud_initialize_backlog = DCloudSwift.master.swiftEventMgr:initializeMaintenanceBacklog',
                    'dcloud_clear_backlog = DCloudSwift.master.swiftEventMgr:clearMaintenanceBacklog',
                    'dcloud_print_backlog = DCloudSwift.util.MaintainReport:print_maintenance_backlog',
                    'dcloud_print_node_info = DCloudSwift.util.MaintainReport:print_node_info',
                    'dcloud_swift_dns_setup = DCloudSwift.master.swiftLoadBalancer:setupDnsRoundRobin',
                    ]
        },

        data_files=[(DELTADIR, ['inputFile.sample', 'Swift.ini', 'swift_master.ini']),
                   ],

        test_suite='unittest',
        long_description=read('README'),
        classifiers=[
                "Development Status :: 3 - Alpha",
                "Topic :: FILESYSTEM",
        ],
        cmdclass={'prepare_image': prepare_image}
    )


    #
    #-- Ovid Wu <ovidwu@gmail.com> Tue, 31 Jul 2012 07:23:25 +0800
    #
    # TODO: move post-installation to debain packaging
    #Post-scripts
    os.system("cp ./Swift.ini  ./src/DCloudSwift/")
    os.system("chmod 755 misc/ServiceScripts/*")
    os.system("cp --preserve misc/ServiceScripts/* /etc/init.d/")

    os.system("update-rc.d -f swift-event-manager remove")
    os.system("update-rc.d swift-event-manager defaults")

    os.system("update-rc.d -f swift-maintain-switcher remove")
    os.system("update-rc.d swift-maintain-switcher defaults")

    os.system("cp ./misc/CronScripts/timesync /etc/cron.hourly")

class prepare_image(setuptools.Command):
    user_options = []
    boolean_options = []
    description = "Copy to /usr/local/src/"

    def initialize_options(self):
        pass

    def finalize_options(self):
        pass

    def run(self):
        if os.path.exists("/usr/src/DCloudSwift"):
            cmd = "rm -rf /usr/local/src/DCloudSwift"
            print cmd
            os.system(cmd)

        cmd = "cp -r %s /usr/local/src/" % WORKING_DIR
        print cmd
        os.system(cmd)
        
        #replace /etc/rc.local with misc/BootScripts/rc.local when building golden image.
        cmd = "cp misc/BootScripts/rc.local /etc/rc.local"
        print cmd
        os.system(cmd)

        # Copy misc/BootScripts/rc.py to /etc/rc.py
        cmd = "cp misc/BootScripts/rc.py /etc/rc.py"
        print cmd
        os.system(cmd)

        # Copy misc/BootScripts/network_setup.py to /etc/networ_setup.py
        cmd = "cp misc/BootScripts/network_setup.py /etc/network_setup.py"
        print cmd
        os.system(cmd)
        
        # Create directories
        if not os.path.exists("/srv"):
            cmd = "mkdir /srv"
            print cmd
            os.system(cmd)

        if not os.path.exists("/DCloudSwift"):
            cmd = "mkdir /DCloudSwift"
            print cmd
            os.system(cmd)

        exit(0)

if __name__ == '__main__':
    main()
