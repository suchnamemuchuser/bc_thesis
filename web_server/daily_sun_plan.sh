#!/bin/bash

PYTHON="/var/www/html/bc_thesis_web/.venv/bin/python3"
SCRIPT="/var/www/html/bc_thesis_web/python/plan_sun.py"
LOGFILE="/var/www/html/logs/sun_planner.log"

# change to project dir for python relative path
cd /var/www/html

$PYTHON $SCRIPT >> $LOGFILE 2>&1