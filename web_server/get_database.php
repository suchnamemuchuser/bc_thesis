<?php
// 1. Get the client's current version from the URL (e.g., ?v=4)
$clientVersion = isset($_GET['v']) ? (int)$_GET['v'] : 0;

// Connect to your database
$dbPath = '../../plan.db';
$pdo = new PDO('sqlite:' . $dbPath);
$pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);

// 2. Check the current server version
$stmt = $pdo->query('SELECT version FROM db_metadata WHERE id = 1');
$serverVersion = (int)$stmt->fetchColumn();

// 3. If versions match, send a "304 Not Modified" and stop.
// This saves massive amounts of bandwidth.
if ($clientVersion === $serverVersion) {
    http_response_code(304);
    exit;
}

// 4. Versions don't match. We need to send the database.
// Create a temporary file path for our safe snapshot
$tempFile = tempnam(sys_get_temp_dir(), 'db_snap_');
unlink($tempFile); // SQLite requires the target file to NOT exist yet

// 5. Use VACUUM INTO to create a perfectly safe, defragmented backup
// This merges the WAL file and main DB into a single new file instantly
$pdo->exec("VACUUM INTO '$tempFile'");

// 6. Send the file to the C client
header('Content-Type: application/vnd.sqlite3');
header('Content-Disposition: attachment; filename="plan_v' . $serverVersion . '.sqlite"');
header('Content-Length: ' . filesize($tempFile));

// We also pass the new version in the HTTP Headers! 
// This makes it easy for your C client to read the ID without querying the DB yet.
header('X-Database-Version: ' . $serverVersion);

// Stream the file to the client
readfile($tempFile);

// 7. Clean up the temporary file so we don't fill up the server hard drive
unlink($tempFile);
?>