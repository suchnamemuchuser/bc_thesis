<?php

// python executable for python scripts, from system or venv
$python = '../../bc_thesis_web/.venv/bin/python3';

$gen_obs_json_py = 'gen_obs_json.py';

$process_log = '/var/lib/rt2/process_log.txt';

// timezone
$localTz = new DateTimeZone('Europe/Prague');

$dbPath = '/var/lib/rt2/plan.db';

?>