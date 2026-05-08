document.addEventListener("DOMContentLoaded", function() {
    
    function updateDashboard() {
        const timestamp = new Date().getTime(); 
        
        document.getElementById('radar-field').src = RADAR_IMAGE_PATH + '?t=' + timestamp;
        document.getElementById('spectrogram').src = SPECTROGRAM_IMAGE_PATH + '?t=' + timestamp;

        fetch('get_current_rec.php')
            .then(response => response.json())
            .then(data => {
                document.getElementById('target-name').innerText = data.target;
            })
            .catch(error => console.error('Error fetching data:', error));
    }

    updateDashboard();

    setInterval(updateDashboard, REFRESH_RATE_MS);
});