#! /bin/sh

if test -z "$1" -o -z "$2"; then
    echo Args [olddir] [newdir] needed.
    exit 1
fi

rm -f count_missing.tmp
files=$(pushd "$1" >/dev/null; ls -1; popd >/dev/null)
for f in $files; do
    diff "$1"/$f "$2"/$f | grep '^< ' | sed 's/< //' >>count_missing.tmp
done

cat count_missing.tmp | sort | uniq -c | sort -k 2 | sort -n -s -r
rm -f count_missing.tmp
