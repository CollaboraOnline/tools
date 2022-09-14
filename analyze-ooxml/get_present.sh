#! /bin/sh

rm -rf txts
mkdir txts
for f in *.docx.dir *.xlsx.dir *.pptx.dir; do
    txt=$(echo $f | sed 's/.dir//').txt
    ./a.out -e -n $f >txts/$txt
done
