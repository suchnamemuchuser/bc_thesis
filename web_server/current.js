document.addEventListener("DOMContentLoaded", function() {
    
    function updateDashboard() {
        const timestamp = new Date().getTime(); 
        
        // Update images
        document.getElementById('radar-field').src = RADAR_IMAGE_PATH + '?t=' + timestamp;
        document.getElementById('spectrogram').src = SPECTROGRAM_IMAGE_PATH + '?t=' + timestamp;

        // Fetch data
        fetch('get_current_rec.php')
            .then(response => response.json())
            .then(data => {
                // Update target name
                document.getElementById('target-name').innerText = data.target;

                // Check if we actually have a target
                if (data.details) {
                    // Convert timestamps to Date
                    const recStart = new Date(data.details.rec_start_time * 1000);
                    const endTime = new Date(data.details.end_time * 1000);

                    // format
                    const timeOptions = { hour: '2-digit', minute: '2-digit' };

                    document.getElementById('target-rec-start').innerText = recStart.toLocaleTimeString([], timeOptions);
                    document.getElementById('target-end').innerText = endTime.toLocaleTimeString([], timeOptions);

                    const isRecording = data.details.record == 1;
                    document.getElementById('target-status').innerText = isRecording ? "Recording" : "Observing";
                    
                } else {
                    // Reset fields if there is no active observation
                    document.getElementById('target-rec-start').innerText = "-";
                    document.getElementById('target-end').innerText = "-";
                    document.getElementById('target-status').innerText = "Idle";
                }
            })
            .catch(error => {
                console.error('Error fetching data:', error);
                document.getElementById('target-name').innerText = "Error fetching data";
            });
    }

    // Initial call
    updateDashboard();

    // Loop
    setInterval(updateDashboard, REFRESH_RATE_MS);
});