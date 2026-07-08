#!/bin/bash

mkdir -p $PWD/.vscode
cp ./$(dirname "$0")/vscode/c_cpp_properties.json $PWD/.vscode/.
