<?php
header('Content-Type: application/json');
require 'config.php';
require 'connect_db.php';

$page = isset($_GET['page']) ? max(1, intval($_GET['page'])) : 1;
$limit = $itemsPerPage; // Pulled directly from config.php
$filter = isset($_GET['filter']) ? $_GET['filter'] : 'all';
$offset = ($page - 1) * $limit;

$now = time();
$whereClause = "1=1";
$params = [];

// filter
if ($filter === 'future') {
    $whereClause = "rec_start_time > ?";
    $params[] = $now;
} elseif ($filter === 'past') {
    $whereClause = "rec_start_time <= ?";
    $params[] = $now;
}

try {
    // total pages
    $countStmt = $pdo->prepare("SELECT COUNT(*) FROM plan WHERE $whereClause");
    $countStmt->execute($params);
    $totalItems = $countStmt->fetchColumn();
    $totalPages = ceil($totalItems / $limit);

    // this page
    $sql = "SELECT * FROM plan WHERE $whereClause ORDER BY rec_start_time DESC LIMIT $limit OFFSET $offset";
    $stmt = $pdo->prepare($sql);
    $stmt->execute($params);
    $plans = $stmt->fetchAll(PDO::FETCH_ASSOC);

    // check if files exist
    foreach ($plans as &$plan) {
        $plan['has_files'] = false;
        $plan['dir_url'] = '';

        $dt = new DateTime();
        $dt->setTimestamp($plan['rec_start_time']);
        $dt->setTimezone($localTz);
        $timeString = $dt->format('Y.m.d-H:i');

        $searchDir = $recordingBaseDir . '/' . $plan['object_name'] . '/';
        $pattern = $searchDir . $timeString . '*';

        $matches = glob($pattern);

        if ($matches && count($matches) > 0) {
            $plan['has_files'] = true;
            $plan['dir_url'] = $alias . '/' . rawurlencode($plan['object_name']) . '/';
        }
    }

    echo json_encode([
        'success' => true, 
        'data' => $plans,
        'pagination' => [
            'currentPage' => $page,
            'totalPages' => $totalPages
        ]
    ]);

} catch (Exception $e) {
    echo json_encode(['success' => false, 'error' => $e->getMessage()]);
}
?>