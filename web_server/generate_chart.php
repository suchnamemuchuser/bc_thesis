<?php
$jsonInput = file_get_contents('php://input');
$data = json_decode($jsonInput, true);

if (!$data) {
    http_response_code(400);
    die("Invalid JSON payload");
}

$leftMargin = 150;
$timelineWidth = 24 * 60 + 1;
$width = $leftMargin + $timelineWidth;

$rowHeight = 40;
$axisHeight = 40;
$height = (count($data) * $rowHeight) + $axisHeight;


$image = imagecreatetruecolor($width, $height);

$barColors = [
    [255, 173, 173],
    [255, 214, 165],
    [253, 255, 182],
    [202, 255, 191],
    [155, 246, 255],
    [160, 196, 255],
    [189, 178, 255],
    [255, 198, 255],
];

$barColorsCount = count($barColors);


$bgColor   = imagecolorallocate($image, 255, 255, 255);
$textColor = imagecolorallocate($image, 50, 50, 50);
$gridColor = imagecolorallocate($image, 230, 230, 230);
$axisColor = imagecolorallocate($image, 150, 150, 150);
$black     = imagecolorallocate($image, 0, 0, 0);

imagefill($image, 0, 0, $bgColor);

$chartBottom = $height - $axisHeight;
imageline($image, $leftMargin, $chartBottom, $width, $chartBottom, $axisColor);

for ($hour = 0; $hour < 24; $hour += 2) {
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

$yOffset = 0;
foreach ($data as $index => $target) {
    imagestring($image, 6, 15, $yOffset + 12, $target['name'], $textColor);
    
    $barColor = imagecolorallocate($image, ...$barColors[$index % $barColorsCount]);

    foreach ($target['windows'] as $window) {
        list($startH, $startM) = explode(':', $window['start']);
        list($endH, $endM) = explode(':', $window['end']);

        $startX = $leftMargin + ($startH * 60) + $startM;
        $endX = $leftMargin + ($endH * 60) + $endM;
        
        $barTop = $yOffset + 8;
        $barBottom = $yOffset + $rowHeight - 8;
        
        if ($endX < $startX) {
            imagefilledrectangle($image, $startX, $barTop, $leftMargin + 1440, $barBottom, $barColor);
            imagerectangle($image, $startX, $barTop, $leftMargin + 1440, $barBottom, $black);
            imagefilledrectangle($image, $leftMargin, $barTop, $endX, $barBottom, $barColor);
            imagerectangle($image, $leftMargin, $barTop, $endX, $barBottom, $black);
        } else {
            imagefilledrectangle($image, $startX, $barTop, $endX, $barBottom, $barColor);
            imagerectangle($image, $startX, $barTop, $endX, $barBottom, $black);
        }
    }
    
    imageline($image, 0, $yOffset + $rowHeight, $width, $yOffset + $rowHeight, $gridColor);
    
    $yOffset += $rowHeight;
}

header('Content-Type: image/png');
imagepng($image);
imagedestroy($image);
?>