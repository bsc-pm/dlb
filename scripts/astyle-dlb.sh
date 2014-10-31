#/bin/bash

OPTIONS="--style=java --add-brackets --keep-one-line-blocks --convert-tabs --suffix=none"

for file in $(find . -name "*.h"); do
    astyle $OPTIONS $file
done

for file in $(find . -name "*.c"); do
    astyle $OPTIONS $file
done
