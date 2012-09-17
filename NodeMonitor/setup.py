import os
import subprocess
from setuptools import setup, find_packages

WORKING_DIR = os.path.dirname(os.path.realpath(__file__))
BASEDIR = os.path.dirname(os.path.dirname(WORKING_DIR))
os.chdir(WORKING_DIR)

DELTADIR = "/etc/delta"

def usage():
    print "usage: python setup.py install\n"


def read(fname):
    return open(os.path.join(os.path.dirname(__file__), fname)).read()


def main():
    os.system("mkdir -p %s" % DELTADIR)

    setup(
        name="Node Monitor",
        version="0.1",
        author="Cloud Data Team, CTBD, Delta Electronic Inc.",
        description=("Delta Inc. CTBD node monitor"),
        license="Delta Inc.",
        keywords=['node', 'disk', 'monitor'],

        packages=find_packages('src'),  # include all packages under src
        package_dir={'': 'src'},   # tell distutils packages are under src
        package_data={
            # If any package contains *.txt or *.rst files, include them:
            '': ['*.txt', '*.rst', '*.sh', "*.ini"],
        },

        scripts=[
        'bin/node-monitor'
        ],

        data_files=[(DELTADIR, ['node_monitor.ini',]),
                   ],

        test_suite='unittest',
        long_description=read('README'),
        classifiers=[
                "Development Status :: 3 - Alpha",
                "Topic :: FILESYSTEM",
        ],
    )

    #Post-scripts
    os.system("chmod 755 misc/ServiceScripts/*")
    os.system("cp --preserve misc/ServiceScripts/* /etc/init.d/")
    os.system("cp --preserve misc/UpstartScripts/* /etc/init/")
    #os.system("update-rc.d -f node-monitor remove")
    #os.system("update-rc.d node-monitor defaults")

if __name__ == '__main__':
    main()
