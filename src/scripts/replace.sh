#!/bin/bash

for files in *.c;do
mv $files $files.old
sed 's/SUPERINODE/SUPERBLOCK/g' $files.old > $files
rm $files.old
done

for files in *.h;do
mv $files $files.old
sed 's/SUPERINODE/SUPERBLOCK/g' $files.old > $files
rm $files.old
done

