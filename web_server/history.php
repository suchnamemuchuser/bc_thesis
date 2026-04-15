<html>
    <head>
        <link rel="stylesheet" href="style.css">
        <title>Observation History & Future Plans</title>
        <style>
            .filter-btn { 
                padding: 6px 16px; 
                margin-right: 5px; 
                cursor: pointer; 
                border: 1px solid #ccc;
                background: #fff;
            }
            .filter-btn.active { 
                font-weight: bold; 
                background-color: #25daa5;
                color: white;
                border-color: #25daa5;
            }
        </style>
    </head>
    <body>
        <a href="index.php" style="text-decoration: none; color: #333;">&larr; Back to Planner</a>
        <h1>All Planned Observations</h1>

        <div style="margin-bottom: 20px;">
            <button class="filter-btn active" data-filter="all">All Recordings</button>
            <button class="filter-btn" data-filter="future">Future Only</button>
            <button class="filter-btn" data-filter="past">Past Only</button>
        </div>

        <table style="width:100%; border-collapse: collapse; border: 1px solid #ccc;">
            <thead>
                <tr style="background: #f0f0f0;">
                    <th>Target</th>
                    <th>ID</th>
                    <th>Observation Start (OST)</th>
                    <th>Recording Start (RST)</th>
                    <th>End Time (ET)</th>
                    <th>Status</th>
                </tr>
            </thead>
            <tbody id="history-table-body">
                <tr><td colspan="6" style="text-align: center; padding: 15px;">Loading data...</td></tr>
            </tbody>
        </table>

        <script src="history.js"></script>
    </body>
</html>