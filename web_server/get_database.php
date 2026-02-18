<?php
$clientVersion = isset($_GET['v']) ? (int)$_GET['v'] : 0;

$dbPath = '../../plan.db';
$pdo = new PDO('sqlite:' . $dbPath);
$pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);

$stmt = $pdo->query('SELECT version FROM db_metadata WHERE id = 1');
$serverVersion = (int)$stmt->fetchColumn();

$stmt->closeCursor(); 
$stmt = null;

if ($clientVersion === $serverVersion) {
    http_response_code(304);
    exit;
}

$tempFile = dirname($dbPath) . '/snapshot_' . uniqid() . '.sqlite';

try {
    $pdo->exec("VACUUM INTO '$tempFile'");
} catch (PDOException $e) {
    http_response_code(500);
    die("Database Error: " . $e->getMessage());
}

header('Content-Type: application/vnd.sqlite3');
header('Content-Disposition: attachment; filename="plan_v' . $serverVersion . '.sqlite"');
header('Content-Length: ' . filesize($tempFile));

header('X-Database-Version: ' . $serverVersion);

readfile($tempFile);

unlink($tempFile);
?>