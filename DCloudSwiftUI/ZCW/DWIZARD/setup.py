from distutils.core import setup
from setuptools import find_packages

setup(name='delta_wizard',
      version='0.1',
      author=u'Delta Electronics, Inc.',
      author_email='delta.cloud.manager@gmail.com',
      packages=["delta", "delta.wizard"],
      url='http://www.deltaww.com.tw',
      description='Delta Zone Config Wizard Framework',
      long_description="",
      include_package_data=True,
      namespace_packages=["delta"],
      zip_safe=False,
      classifiers=['Development Status :: Beta',
                   'Framework :: Django',
                   'Intended Audience :: Developers',
                   'Operating System :: OS Independent',
                   'Programming Language :: Python',
                   'Topic :: Internet :: WWW/HTTP']
)
