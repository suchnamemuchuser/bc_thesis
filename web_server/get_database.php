<?php
require_once 'connect_db.php'; 

$clientVersion = isset($_GET['v']) ? (int)$_GET['v'] : 0;

$stmt = $pdo->query('SELECT version FROM db_metadata WHERE id = 1');
$serverVersion = (int)$stmt->fetchColumn();

if ($clientVersion === $serverVersion) {
    http_response_code(204);
    exit;
}

header('Content-Type: text/plain');
header('X-Database-Version: ' . $serverVersion);


$threshold = time() - 24 * 60 * 60;


$response = "BEGIN TRANSACTION;\n";
$response .= "DELETE FROM recording_plan;\n";

$stmt = $pdo->query('SELECT id, object_name, is_interstellar, obs_start_time, rec_start_time, end_time FROM plan WHERE end_time > :threshold');
$stmt->bindValue(':threshold', $threshold, PDO::PARAM_INT);
$stmt->execute();

while ($row = $stmt->fetch(PDO::FETCH_ASSOC)) {
    $id = (int)$row['id'];
    $objName = $pdo->quote($row['object_name']);
    $isInterstellar = (int)$row['is_interstellar'];
    $obsStartTime = (int)$row['obs_start_time'];
    $recStartTime = (int)$row['rec_start_time'];
    $endTime = (int)$row['end_time'];

    $response .= "INSERT INTO recording_plan (id, start_time, duration, channel) ";
    $response .= "VALUES ($id, $objName, $isInterstellar, $obsStartTime, $recStartTime, $endTime);\n";
}

$response .= "UPDATE db_metadata SET version = $serverVersion WHERE id = 1;\n";
$response .= "COMMIT;";

echo $response;
?>