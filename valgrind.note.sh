#!/usr/bin/bash

# --show-reachable=yes

valgrind --read-var-info=yes --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose ./build/bin/propd --loglevel=DEBUG --unix long,file,* --file /tmp/propd.file.c/,file.c,c.*
valgrind --read-var-info=yes --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose ./build/bin/propd --loglevel=DEBUG --name file --file /tmp/propd.file.a/,file.a,a.* --file /tmp/propd.file.b/,file.b,b.* --prefix a.*,b.*
