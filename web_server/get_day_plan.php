<?php

header('Content-Type: application/json');

require_once 'connect_db.php';

require_once 'config.php';

$todayObj = new DateTime('now', $localTz);

$todayDate = $todayObj->format('Y-m-d');

$date = isset($_GET['date']) ? $_GET['date'] : $todayDate;

try
{
    $sql = 'SELECT
                object_name, 
                strftime(\'%H:%M\', obs_start_time, \'unixepoch\', \'localtime\') AS obs_start_time, 
                strftime(\'%H:%M\', rec_start_time, \'unixepoch\', \'localtime\') AS rec_start_time, 
                strftime(\'%H:%M\', end_time, \'unixepoch\', \'localtime\') AS end_time,
                date(obs_start_time, \'unixepoch\', \'localtime\') AS start_date,
                date(end_time, \'unixepoch\', \'localtime\') AS end_date
            FROM plan 
            WHERE date(obs_start_time, \'unixepoch\', \'localtime\') = :date1
            OR date(end_time, \'unixepoch\', \'localtime\') = :date2
            ORDER BY obs_start_time ASC';

    $stmt = $pdo->prepare($sql);
    $stmt->bindValue(':date1', $date, PDO::PARAM_STR);
    $stmt->bindValue(':date2', $date, PDO::PARAM_STR);
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