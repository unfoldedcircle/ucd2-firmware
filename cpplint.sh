#! /bin/bash
cpplint --filter=-build/include_subdir,-build/c++11 --linelength=120 --exclude=lib/Update  --root=lib --recursive lib src
