<?php

header('Content-Type: application/json');

require_once 'connect_db.php';

require_once 'config.php';

$todayObj = new DateTime('now', $localTz);

$todayDate = $todayObj->format('Y-m-d');

$date = isset($_GET['date']) ? $_GET['date'] : $todayDate;

$givenDate = new DateTime($date, $localTz);
$startDate = $givenDate->modify('-1 day')->format('Y-m-d');
$endDate = $givenDate->modify('+2 days')->format('Y-m-d');

try
{
    $sql = "SELECT
                id,
                object_name,
                obs_start_time,
                rec_start_time,
                end_time
            FROM plan
            WHERE date(obs_start_time, 'unixepoch', 'localtime') BETWEEN :startDate AND :endDate
            OR date(end_time, 'unixepoch', 'localtime') BETWEEN :startDate AND :endDate
            ORDER BY obs_start_time ASC";

    $stmt = $pdo->prepare($sql);
    $stmt->bindValue(':startDate', $startDate, PDO::PARAM_STR);
    $stmt->bindValue(':endDate', $endDate, PDO::PARAM_STR);
    $stmt->execute();
    
    $plans = $stmt->fetchAll(PDO::FETCH_ASSOC);

    echo json_encode([
        'success' => true,
        'data' => $plans
    ]);
}
catch (Throwable $e)
{
    http_response_code(500);
    echo json_encode([
        'success' => false, 
        'error' => $e->getMessage(),
        'line' => $e->getLine(),
        'file' => $e->getFile()
    ]);
}
?>