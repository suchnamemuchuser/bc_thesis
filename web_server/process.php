<?php

header('Content-Type: application/json');

require_once 'config.php';

if ($_SERVER['REQUEST_METHOD'] === 'POST')
{
    $date = $_POST['date'] ?? '';
    $raw_targets = $_POST['targets'] ?? [];

    if (empty($date) || empty($raw_targets))
    {
        echo json_encode(["error" => "Something went wrong!"]);
        exit;
    }

    $final_targets = [];

    foreach ($raw_targets as $target)
    {
        if (isset($target['enabled']))
        {
            $final_targets[] = $target['name'] . ":" . $target['type'];
        }
    }

    if(empty($final_targets))
    {
        echo json_encode(["error" => "No targets selected!"]);
        exit;
    }

    $targets_string = implode(',', $final_targets);

    $safe_date = escapeshellarg($date);
    $safe_targets = escapeshellarg($targets_string);

    $command = "$python $gen_obs_json_py $safe_date $safe_targets 2>&1";

    exec($command, $output_array, $return_code);

    if (count($output_array) != 1 || $return_code != 0)
    {
        file_put_contents($process_log, print_r(implode("\r\n", $output_array)."\r\n", true));
    }
    

    $json_string = '';
    foreach ($output_array as $line)
    {
        if (strpos($line, '{"date":') === 0)
            {
            $json_string = $line;
            break;
        }
    }
    echo $json_string;
}
else
{
    echo json_encode(["error" => "Invalid request method"]);
}

?>