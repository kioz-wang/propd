# Memory IO

> MIT License Copyright (c) 2025 kioz.wang

# Description

基于内存布局描述执行内存访问。

## Test and Run

```shell
$ rm -f *.gch *.o a.out && gcc -fsanitize=leak *.[ch] -lcjson
```

## Build as library

If you want, just do it.

## Build with your source codes

This is a best practice. Add `list_search.c/h` into your source tree if you want to use `list_search`; add `position.c/h` additionally if use `pos_read/write`; add `layout.c/h` additionally and link `cjson` if use `layout_parse`.
