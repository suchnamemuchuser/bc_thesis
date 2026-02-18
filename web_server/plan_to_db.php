<?php

header('Content-Type: application/json');

try {
    $input = json_decode(file_get_contents('php://input'), true);

    if (!$input) {
        throw new Exception("No data received");
    }

    $db = '../../plan.db';
    $dsn = "sqlite:$db";
    $pdo = new PDO($dsn);
    $pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);

    $objectName = $input['object_name'];
    $location = $input['location'];

    $isInterstellar = ($location === 'solar') ? 0 : 1;

    $recStartTime = (int)$input['rec_start_time']; 
    $endTime = (int)$input['end_time'];           
    
    $obsStartTime = $recStartTime - 600; 

    $checkSql = 'SELECT object_name, obs_start_time, end_time 
                 FROM plan 
                 WHERE obs_start_time <= :new_end 
                 AND end_time >= :new_start';
                 
    $checkStmt = $pdo->prepare($checkSql);
    $checkStmt->bindValue(':new_end', $endTime);
    $checkStmt->bindValue(':new_start', $obsStartTime);
    $checkStmt->execute();
    
    $overlapCount = $checkStmt->fetchColumn();

    $overlaps = $checkStmt->fetchAll(PDO::FETCH_ASSOC);

    if (!empty($overlaps)) {
        $conflictDetails = [];
        
        foreach ($overlaps as $row) {
            $startStr = date('H:i', $row['obs_start_time']);
            $endStr = date('H:i', $row['end_time']);
            
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

} catch (Exception $e) {
    http_response_code(500);
    echo json_encode(['error' => $e->getMessage()]);
}
?>