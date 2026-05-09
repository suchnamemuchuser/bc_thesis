<?php

header('Content-Type: application/json');

require_once 'config.php';
require_once 'connect_db.php';

try
{
    $input = json_decode(file_get_contents('php://input'), true);

    if (!$input || !isset($input['id']))
    {
        throw new Exception('No id provided');
    }

    $id = (int)$input['id'];
    $currentTime = time();

    // ensure both insert and delete succeed together
    $pdo->beginTransaction();

    // get existing plan
    $stmt = $pdo->prepare('SELECT * FROM plan WHERE id = :id');
    $stmt->bindValue(':id', $id, PDO::PARAM_INT);
    $stmt->execute();
    $plan = $stmt->fetch(PDO::FETCH_ASSOC);

    if (!$plan)
    {
        throw new Exception('Plan not found');
    }

    // if recording active
    if ($currentTime >= $plan['obs_start_time'] && $currentTime < $plan['end_time'])
    {

        // create duplicate with different id and current time as end time
        $insertSql = 'INSERT INTO plan (object_name, is_interstellar, obs_start_time, rec_start_time, end_time, record) 
                      VALUES (:object_name, :is_interstellar, :obs_start_time, :rec_start_time, :end_time, :record)';

        $insertStmt = $pdo->prepare($insertSql);
        $insertStmt->execute([
            ':object_name'     => $plan['object_name'],
            ':is_interstellar' => $plan['is_interstellar'],
            ':obs_start_time'  => $plan['obs_start_time'],
            ':rec_start_time'  => $plan['rec_start_time'],
            ':end_time'        => $currentTime,
            ':record'          => $plan['record']
        ]);
    }

    // delete original item
    $deleteSql = 'DELETE FROM plan WHERE id = :id';
    $deleteStmt = $pdo->prepare($deleteSql);
    $deleteStmt->bindValue(':id', $id, PDO::PARAM_INT);
    $deleteStmt->execute();

    // commit transaction
    $pdo->commit();

    echo json_encode(['success' => true]);

}
catch (Throwable $e)
{
    if (isset($pdo) && $pdo->inTransaction())
    {
        $pdo->rollBack();
    }
    
    http_response_code(500);
    echo json_encode([
        'success' => false,
        'error' => $e->getMessage()
    ]);
}

?>