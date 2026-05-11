<?php require_once 'config.php'; ?>

<html>
    <head>
        <link rel="stylesheet" href="style.css">
        <title>Current Observation</title>
    </head>
    <body>
        <?php $pageTitle = "Current Observation"; include 'header.php'; ?>
        
        <section id="current-recording">
            <div class="dashboard-panel">
                <h3>Current Target: <span id="target-name">Loading...</span></h3>
                <ul class="target-details">
                    <li><strong>Status:</strong> <span id="target-status">-</span></li>
                    <li><strong>Start:</strong> <span id="target-rec-start">-</span></li>
                    <li><strong>End:</strong> <span id="target-end">-</span></li>
                </ul>
            </div>

            <div class="image-container">
                <img id="spectrogram" src="<?php echo $spectrogram; ?>" alt="spectrogram">
                <img id="radar-field" src="<?php echo $radarField; ?>" alt="radar field">
            </div>
        </section>

        <script>
            const REFRESH_RATE_MS = <?php echo isset($refreshIntervalSeconds) ? ($refreshIntervalSeconds * 1000) : 10000; ?>;
            const RADAR_IMAGE_PATH = "<?php echo $radarField; ?>";
            const SPECTROGRAM_IMAGE_PATH = "<?php echo $spectrogram; ?>"; 
        </script>
        <script src="current.js"></script>
    </body>
</html>