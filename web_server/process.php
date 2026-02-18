<?php

header('Content-Type: application/json');

$python = "../../bc_thesis_web/.venv/bin/python3";
$script = "gen_obs_json.py";


if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    $date = $_POST['date'] ?? '';
    $raw_targets = $_POST['targets'] ?? [];

    if (empty($date) || empty($raw_targets)) {
        echo json_encode(["error" => "Something went wrong!"]);
        exit;
    }

    $final_targets = [];

    foreach ($raw_targets as $target){
        if (isset($target['enabled'])) {
            $final_targets[] = $target['name'] . ":" . $target['type'];
        }
    }

    if(empty($final_targets)){
        echo json_encode(["error" => "No targets selected!"]);
        exit;
    }

    $targets_string = implode(',', $final_targets);

    // 1. Sanitize inputs for the shell
    $safe_date = escapeshellarg($date);
    $safe_targets = escapeshellarg($targets_string);

    $command = "$python $script $safe_date $safe_targets 2>&1";

    exec($command, $output_array, $return_code);

    file_put_contents("{$_SERVER["DOCUMENT_ROOT"]}/logs/debug.txt", print_r(implode("\r\n", $output_array), true));

    $json_string = '';
    foreach ($output_array as $line) {
        if (strpos($line, '{"date":') === 0) {
            $json_string = $line;
            break;
        }
    }
    echo $json_string;
} else {
    echo json_encode(["error" => "Invalid request method"]);
}

?>