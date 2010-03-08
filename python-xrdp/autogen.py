#! /usr/bin/python

import os
import pysvn

# Detect the version
if os.environ.has_key("OVD_VERSION"):
    version = os.environ["OVD_VERSION"]

else:
    c = pysvn.Client()
    revision = "%05d" % (c.info(".")["revision"].number)
    version = "99.99~trunk+svn%s"%(revision)

f = file("setup.py.in", "r")
content = f.read()
f.close()

content = content.replace("@REVISION@", str(revision))
content = content.replace("@VERSION@", str(version))

f = file("setup.py", "w")
f.write(content)
f.close()
