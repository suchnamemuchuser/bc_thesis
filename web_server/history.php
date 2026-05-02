<?php require_once 'config.php'; ?>

<html>
    <head>
        <link rel="stylesheet" href="style.css">
        <title>Observations</title>
    </head>
    <body>
        <?php $pageTitle = "Observations"; include 'header.php'; ?>
        <section id="history">
            <div style="margin-bottom: 20px;">
                <button class="filter-btn active" data-filter="all">All Recordings</button>
                <button class="filter-btn" data-filter="future">Future Only</button>
                <button class="filter-btn" data-filter="past">Past Only</button>
            </div>

            <table>
                <thead>
                    <tr>
                        <th>Target</th>
                        <th>ID</th>
                        <th>Observation Start (OST)</th>
                        <th>Recording Start (RST)</th>
                        <th>End Time (ET)</th>
                        <th>Status</th>
                        <th style="width: 40px;"></th> </tr>
                </thead>
                <tbody id="history-table-body">
                    <tr><td colspan="7" style="text-align: center; padding: 15px;">Loading data...</td></tr>
                </tbody>
            </table>

            <div id="pagination-controls"></div>
        </section>

        <script src="history.js"></script>
    </body>
</html>