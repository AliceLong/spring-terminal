#!/bin/zsh
cd "${0:A:h}"
if [[ -x .venv/bin/python ]]; then spring_python=.venv/bin/python; else spring_python=python3; fi
exec "$spring_python" monitor/server.py 
