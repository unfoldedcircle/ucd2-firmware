# Code Guidelines

Use `clang-format` with:

- Google style
- indent: 4
- column width: 120

Configuration file is included in the project root directory: [`.clang-format`](../.clang-format).

Auto-format in main project directory:
```shell
clang-format -i lib/log/*.*
```

Coding style is enfored in GitHub CI build.  
Manual check:
```shell
cpplint --filter=-build/include_subdir,-build/c++11 --linelength=120 --exclude=lib/Update  --root=lib --recursive lib src
```

Excluded libs:

- [lib/Update/](../lib/Update/): 3rd party code
