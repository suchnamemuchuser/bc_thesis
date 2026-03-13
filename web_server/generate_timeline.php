<?php
require_once 'config.php';

$payload = json_decode(file_get_contents('php://input'), true);
$targetDate = $payload['targetDate'];
$data = $payload['plans'];

if (!is_array($data)) {
    http_response_code(400);
    die("Invalid JSON payload");
}

// Helper function to convert Unix timestamp (UTC) to local DateTime
function utcToLocal($unixTimestamp, $localTz) {
    $dt = new DateTime('@' . $unixTimestamp, new DateTimeZone('UTC'));
    $dt->setTimezone($localTz);
    return $dt;
}

// Parse target date and create 3-day window
$targetDt = DateTime::createFromFormat('Y-m-d', $targetDate, $localTz);
if (!$targetDt) {
    http_response_code(400);
    die("Invalid target date format");
}

// Window: day before, target day, day after (72 hours starting at 00:00 local time of day before)
$windowStart = clone $targetDt;
$windowStart->modify('-1 day');
$windowStart->setTime(0, 0, 0);

$windowEnd = clone $windowStart;
$windowEnd->modify('+3 days');
$windowEnd->setTime(0, 0, 0);

$leftMargin = 150;
$timelineWidth = 3 * 24 * 20 + 1;
$width = $leftMargin + $timelineWidth;

$rowHeight = 60;
$axisHeight = 40;
$height = $rowHeight + $axisHeight;

$image = imagecreatetruecolor($width, $height);

$barColors = [
    [160, 196, 255],
    [189, 178, 255],
    [255, 198, 255],
    [155, 246, 255],
    [253, 255, 182]
];
$barColorsCount = count($barColors);

$bgColor   = imagecolorallocate($image, 255, 255, 255);
$textColor = imagecolorallocate($image, 50, 50, 50);
$gridColor = imagecolorallocate($image, 230, 230, 230);
$axisColor = imagecolorallocate($image, 150, 150, 150);
$stripeColor = imagecolorallocate($image, 255, 255, 255);
$black     = imagecolorallocate($image, 0, 0, 0);
$red       = imagecolorallocate($image, 255, 0, 0);
$lightGrey = imagecolorallocate($image, 200, 200, 200);

imagefilledrectangle($image, 0, 0, $width, $height, $bgColor);

$chartBottom = $rowHeight;
imageline($image, $leftMargin, $chartBottom, $width, $chartBottom, $axisColor);

for ($hour = 0; $hour <= 72; $hour += 2) {
    $x = $leftMargin + ($hour * 20);
    
    if ($hour < 72) {
        imageline($image, $x, 0, $x, $chartBottom, $gridColor);
    }
    
    imageline($image, $x, $chartBottom, $x, $chartBottom + 5, $axisColor);

    $timeLabel = sprintf('%02d:00', $hour % 24);
    
    $textWidth = strlen($timeLabel) * 6;
    imagestring($image, 3, $x - ($textWidth / 2), $chartBottom + 10, $timeLabel, $textColor);
}

imagestring($image, 5, 15, ($rowHeight / 2) - 8, "Daily Plan", $textColor);

// Calculate pixels per second for the 72-hour timeline
$pixelsPerSecond = $timelineWidth / (3 * 24 * 3600);

// Draw current time indicator if it falls within the window
$currentTime = new DateTime('now', $localTz);

$barTop = 10;
$barBottom = $rowHeight - 10;

// Filter and draw events
$eventIndex = 0;
foreach ($data as $row) {
    $obsStartTime = utcToLocal($row['obs_start_time'], $localTz);
    $recStartTime = utcToLocal($row['rec_start_time'], $localTz);
    $endTime = utcToLocal($row['end_time'], $localTz);
    
    // Check if event overlaps with the 3-day window
    if ($endTime < $windowStart || $obsStartTime >= $windowEnd) {
        continue;
    }
    
    // Calculate seconds from window start for each timestamp
    $obsStartSecOffset = $obsStartTime->getTimestamp() - $windowStart->getTimestamp();
    $recStartSecOffset = $recStartTime->getTimestamp() - $windowStart->getTimestamp();
    $endSecOffset = $endTime->getTimestamp() - $windowStart->getTimestamp();
    
    // Clamp to visible window and convert to pixel positions
    $drawStartX = $leftMargin + max(0, $obsStartSecOffset * $pixelsPerSecond);
    $drawEndX = $leftMargin + min($timelineWidth, $endSecOffset * $pixelsPerSecond);
    $drawRecX = $leftMargin + max(0, min($timelineWidth, $recStartSecOffset * $pixelsPerSecond));
    
    // Only draw if visible
    if ($drawEndX <= $drawStartX) {
        continue;
    }
    
    // Get color for this event
    $barColor = imagecolorallocate($image, ...$barColors[$eventIndex % $barColorsCount]);
    $eventIndex++;
    
    // Use light grey if event has ended
    if ($endTime <= $currentTime) {
        $barColor = $lightGrey;
    }
    
    // Draw solid rectangle from obs_start to end
    imagefilledrectangle($image, (int)$drawStartX, $barTop, (int)$drawEndX, $barBottom, $barColor);
    imagerectangle($image, (int)$drawStartX, $barTop, (int)$drawEndX, $barBottom, $black);
    
    // Draw vertical line at recording start time
    if ($drawRecX >= $leftMargin && $drawRecX <= $leftMargin + $timelineWidth) {
        imageline($image, (int)$drawRecX, $barTop, (int)$drawRecX, $barBottom, $black);
    }
    
    // Draw label
    $name = $row['object_name'];
    $textWidth = strlen($name) * 6;
    $totalBlockWidth = $drawEndX - $drawStartX;
    
    if ($totalBlockWidth > $textWidth + 10) {
        $textX = $drawStartX + ($totalBlockWidth / 2) - ($textWidth / 2);
    } else {
        $textX = $drawStartX + 5;
    }
    
    imagestring($image, 3, (int)$textX, $barTop + 13, $name, $textColor);
}

if ($currentTime >= $windowStart && $currentTime < $windowEnd) {
    $currentSecOffset = $currentTime->getTimestamp() - $windowStart->getTimestamp();
    $currentX = $leftMargin + ($currentSecOffset * $pixelsPerSecond);
    imageline($image, (int)$currentX, 0, (int)$currentX, $height, $red);
}

header('Content-Type: image/png');
imagepng($image);
imagedestroy($image);
?>