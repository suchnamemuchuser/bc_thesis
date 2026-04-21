<?php
require_once 'connect_db.php'; 

$clientVersion = isset($_GET['v']) ? (int)$_GET['v'] : 0;

$stmt = $pdo->query('SELECT version FROM db_metadata WHERE id = 1');
$serverVersion = (int)$stmt->fetchColumn();

if ($clientVersion === $serverVersion) {
    http_response_code(204);
    exit;
}

$threshold = time() - 24 * 60 * 60;

$stmt = $pdo->prepare('SELECT id, object_name, is_interstellar, obs_start_time, rec_start_time, end_time, record FROM plan WHERE end_time > :threshold');
$stmt->bindValue(':threshold', $threshold, PDO::PARAM_INT);
$stmt->execute();
$rows = $stmt->fetchAll(PDO::FETCH_ASSOC);

$response = [
    "version" => $serverVersion,
    "data" => $rows
];

header('Content-Type: application/json');
echo json_encode($response);

?>