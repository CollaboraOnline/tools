#! /bin/sh

rm -f enums.tmp 2>/dev/null
for file in "$@"; do
    cat "$file" | grep 'xsd:enumeration' | sed 's#^.*<xsd:enumeration value="\(.*\)"/>.*$#\1#' >>enums.tmp
done

# consider only values that are at least two characters and contain at least one letter
# (to avoid values like '10' or 'a')
sort -u enums.tmp | grep .. | grep [A-Za-z] > enums.txt
rm enums.tmp
