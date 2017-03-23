#!/bin/sh

if [ -z "$MNT_POINT_SD_CARD" ]
then
  echo "Please set the environment variable MNT_POINT_SD_CARD"
  exit 1
fi

cd "$MNT_POINT_SD_CARD"
wget "https://builds.magiclantern.fm/jenkins/job/600D.102/387/artifact/platform/600D.102/*zip*/600D.102.zip"
unzip 600D.102.zip
rm 600D.102.zip

unzip 600D.102/*.zip
rm -rf 600D.102

cd ML/modules
wget "https://builds.magiclantern.fm/jenkins/job/io_crypt/lastBuild/artifact/modules/io_crypt/io_crypt.mo"
wget "https://builds.magiclantern.fm/jenkins/job/ime_base/lastBuild/artifact/modules/ime_base/ime_base.mo"
wget "https://builds.magiclantern.fm/jenkins/job/ime_base/lastBuild/artifact/modules/ime_rot/ime_rot.mo"

umount "$MNT_POINT_SD_CARD"
echo "Installation is done"
exit 0
