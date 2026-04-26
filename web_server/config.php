<?php

$gen_obs_json_py = '/var/www/bin/gen_obs_json.py';

$process_log = '/var/lib/rt2/process_log.txt';

// timezone
$localTz = new DateTimeZone('Europe/Prague');

$dbPath = '/var/lib/rt2/plan.db';

$itemsPerPage = 30;

$predefinedTargets = [
    ['name' => 'Sun',      'type' => 'solar', 'enabled' => true],
    ['name' => 'B0329+54', 'type' => 'deep',  'enabled' => false],
    ['name' => 'B0943+10', 'type' => 'deep',  'enabled' => false],
    ['name' => 'B1957+20', 'type' => 'deep',  'enabled' => false],
    ['name' => 'B1937+21', 'type' => 'deep',  'enabled' => false],
];

$recordingBaseDir = "/var/lib/rt2/data";

$alias = "/data";

?>