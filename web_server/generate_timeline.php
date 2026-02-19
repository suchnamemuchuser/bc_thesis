<?php
$payload = json_decode(file_get_contents('php://input'), true);
$targetDate = $payload['targetDate'];
$data = $payload['plans'];

if (!is_array($data)) {
    http_response_code(400);
    die("Invalid JSON payload");
}

$leftMargin = 150;
$timelineWidth = 24 * 60 + 1;
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

imagefilledrectangle($image, 0, 0, $width, $height, $bgColor);

$chartBottom = $rowHeight;
imageline($image, $leftMargin, $chartBottom, $width, $chartBottom, $axisColor);

for ($hour = 0; $hour <= 24; $hour += 2) {
    $x = $leftMargin + ($hour * 60);
    
    if ($hour < 24) {
        imageline($image, $x, 0, $x, $chartBottom, $gridColor);
    }
    
    imageline($image, $x, $chartBottom, $x, $chartBottom + 5, $axisColor);

    $timeLabel = sprintf('%02d:00', $hour % 24);
    if ($hour == 24) $timeLabel = "24:00"; 
    
    $textWidth = strlen($timeLabel) * 6;
    imagestring($image, 3, $x - ($textWidth / 2), $chartBottom + 10, $timeLabel, $textColor);
}

imagestring($image, 5, 15, ($rowHeight / 2) - 8, "Daily Plan", $textColor);

$barTop = 10;
$barBottom = $rowHeight - 10;

foreach ($data as $index => $row) {
    $barColor = imagecolorallocate($image, ...$barColors[$index % $barColorsCount]);
    
    list($startH, $startM) = explode(':', $row['obs_start_time']);
    list($endH, $endM) = explode(':', $row['end_time']);

    $startMinsRaw = ($startH * 60) + $startM;
    if ($row['start_date'] < $targetDate) {
        $startMinsRaw -= 1440;
    } elseif ($row['start_date'] > $targetDate) {
        $startMinsRaw += 1440;
    }

    $endMinsRaw = ($endH * 60) + $endM;
    if ($row['end_date'] > $targetDate) {
        $endMinsRaw += 1440;
    } elseif ($row['end_date'] < $targetDate) {
        $endMinsRaw -= 1440;
    }

    $recMinsRaw = $startMinsRaw + 10;

    $drawStartMins = max(0, min(1440, $startMinsRaw));
    $drawRecMins   = max(0, min(1440, $recMinsRaw));
    $drawEndMins   = max(0, min(1440, $endMinsRaw));

    $drawStartX = $leftMargin + $drawStartMins;
    $drawRecX   = $leftMargin + $drawRecMins;
    $drawEndX   = $leftMargin + $drawEndMins;

    if ($drawRecX > $drawStartX) {
        imagefilledrectangle($image, $drawStartX, $barTop, $drawRecX, $barBottom, $barColor);
        
        for ($x = $drawStartX + 3; $x < $drawRecX; $x += 4) {
            imageline($image, $x, $barTop, $x, $barBottom, $stripeColor);
        }
        
        imagerectangle($image, $drawStartX, $barTop, $drawRecX, $barBottom, $black);
    }

    if ($drawEndX > $drawRecX) {
        imagefilledrectangle($image, $drawRecX, $barTop, $drawEndX, $barBottom, $barColor);
        imagerectangle($image, $drawRecX, $barTop, $drawEndX, $barBottom, $black);
    }

    $name = $row['object_name'];
    $textWidth = strlen($name) * 6;
    
    $totalBlockWidth = $drawEndX - $drawStartX;
    
    if ($totalBlockWidth > $textWidth + 10) {
        $textX = $drawStartX + ($totalBlockWidth / 2) - ($textWidth / 2);
    } else {
        $textX = $drawStartX + 5;
    }
    
    imagestring($image, 3, $textX, $barTop + 13, $name, $textColor);
}

header('Content-Type: image/png');
imagepng($image);
imagedestroy($image);
?>