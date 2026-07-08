#!/bin/bash

conf_file=$(dirname "$0")/cppcheck.cfg

echo "$(cppcheck --version)"
cppcheck ./cli \
    -I/opt/vzlu/include \
    --enable=all \
    --force \
    --suppress=ConfigurationNotChecked \
    --suppress=unusedFunction \
    --suppress=missingIncludeSystem \
    --suppress=missingInclude \
    --suppress=unmatchedSuppression \
    --suppress=checkersReport \
    --library=$conf_file \
    --error-exitcode=1
