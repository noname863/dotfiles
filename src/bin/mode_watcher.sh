#! /bin/bash
echo `dirname $0`/.mode_watcher
LD_LIBRARY_PATH=$HOME/.local/lib `dirname $0`/mode_watcher
