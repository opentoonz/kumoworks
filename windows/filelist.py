# list up files in program and stuff

import os
import codecs

fout = codecs.open("files.iss", "w", "utf_8_sig")

for path, dirs, files in os.walk(ur'winInstallerTmp/program'):
    for file in files:
        print >> fout, """Source: "%s"; DestDir: "{app}%s"; Flags: ignoreversion""" % (
            os.path.join(path, file),
            path[len("winInstallerTmp/program"):])

fout.close()
