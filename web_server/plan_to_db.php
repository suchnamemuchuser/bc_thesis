<?php

header('Content-Type: application/json');

require_once 'config.php';

require_once 'connect_db.php';

try 
{
    $input = json_decode(file_get_contents('php://input'), true);

    if (!$input)
    {
        throw new Exception("No data received");
    }

    $objectName = $input['object_name'];
    $location = $input['location'];

    $isInterstellar = ($location === 'solar') ? 0 : 1;

    $recStartTime = (int)$input['rec_start_time']; 
    $endTime = (int)$input['end_time'];           
    
    $obsStartTime = $recStartTime - 600; 

    $checkSql = 'SELECT object_name, obs_start_time, end_time 
             FROM plan 
             WHERE obs_start_time <= :new_end 
             AND end_time >= :new_start
             ORDER BY obs_start_time ASC';
             
    $checkStmt = $pdo->prepare($checkSql);
    $checkStmt->bindValue(':new_end', $endTime);
    $checkStmt->bindValue(':new_start', $obsStartTime);
    $checkStmt->execute();

    $overlaps = $checkStmt->fetchAll(PDO::FETCH_ASSOC);

    if (!empty($overlaps)) 
    {
        $conflictDetails = [];
        
        $localTz = new DateTimeZone('Europe/Prague'); 
        
        foreach ($overlaps as $row) 
        {
            $dtStart = new DateTime("@" . $row['obs_start_time']);
            $dtStart->setTimezone($localTz);
            $startStr = $dtStart->format('H:i');
            
            $dtEnd = new DateTime("@" . $row['end_time']);
            $dtEnd->setTimezone($localTz);
            $endStr = $dtEnd->format('H:i');
            
            $name = $row['object_name'];
            $conflictDetails[] = "{$name} ({$startStr} - {$endStr})";
        }
        
        $conflictList = implode(', ', $conflictDetails);
        
        throw new Exception("Time slot overlaps with: " . $conflictList);
    }

    $sql = 'INSERT INTO plan(object_name, is_interstellar, obs_start_time, rec_start_time, end_time)
            VALUES(:object_name, :is_interstellar, :obs_start_time, :rec_start_time, :end_time)';

    $stmt = $pdo->prepare($sql);

    $stmt->bindValue(':object_name', $objectName);
    $stmt->bindValue(':is_interstellar', $isInterstellar);
    $stmt->bindValue(':obs_start_time', $obsStartTime);
    $stmt->bindValue(':rec_start_time', $recStartTime);
    $stmt->bindValue(':end_time', $endTime);

    $stmt->execute();

    echo json_encode(['success' => true, 'id' => $pdo->lastInsertId()]);

}
catch (Exception $e)
{
    http_response_code(500);
    echo json_encode(['success' => false, 'error' => $e->getMessage()]);
}
?>