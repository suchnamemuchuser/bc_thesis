<?php
header('Content-Type: application/json');
require_once 'connect_db.php';

try {
    $now = time(); 

    $stmt = $pdo->prepare("
        SELECT * 
        FROM plan 
        WHERE obs_start_time <= :now 
          AND end_time >= :now 
        LIMIT 1
    ");

    $stmt->execute(['now' => $now]);
    $currentObservation = $stmt->fetch(PDO::FETCH_ASSOC);

    if ($currentObservation) {
        echo json_encode([
            'target' => $currentObservation['object_name'],
            'details' => $currentObservation
        ]);
    } else {
        echo json_encode([
            'target' => 'None',
            'details' => null
        ]);
    }

} catch (PDOException $e) {
    http_response_code(500);
    echo json_encode([
        'target' => 'Error',
        'message' => $e->getMessage()
    ]);
}
?>